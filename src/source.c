#include "source.h"
#include "input.h"

#include <stdio.h>
#include <string.h>

#define CONFIGURING_UNKNOWN 0
#define CONFIGURING_INPUT 1
#define CONFIGURING_DECODER 2
#define CONFIGURING_FILTER 3

static strbuf DEFAULT_FILTER = { .a = 0, .len = 11, .x = (uint8_t*)"passthrough" };

static int source_default_tag_handler(void* ud, const taglist* tags) {
    source *s = (source *)ud;
    return taglist_deep_copy(&s->tagcache,tags);
}

static int source_open_intercept(void* ud, const frame_source* fsource) {
    source *s = (source *)ud;
    s->frame_source = *fsource;
    return 0;
}

/* wrappers to forward calls from the input/decoder to whatever the
 * source is configured to use */
static int source_tag_handler_wrapper(void* ud, const taglist* tags) {
    source *s = (source *)ud;
    return s->tag_handler.cb(s->tag_handler.userdata, tags);
}

static int source_submit_frame_wrapper(void* ud, const frame* frame) {
    source *s = (source *)ud;
    return s->frame_destination.submit_frame(s->frame_destination.handle, frame);
}

static int source_flush_wrapper(void* ud) {
    source *s = (source *)ud;
    return s->frame_destination.flush(s->frame_destination.handle);
}

int source_open_dest(const source* s, const frame_receiver* dest) {
    frame_source me = s->frame_source;
    return dest->open(dest->handle,&me);
}

int source_global_init(void) {
    int r;
    if( (r = input_global_init()) != 0) return r;
    if( (r = decoder_global_init()) != 0) return r;
    if( (r = filter_global_init()) != 0) return r;
    return 0;
}

void source_global_deinit(void) {
    input_global_deinit();
    decoder_global_deinit();
    filter_global_deinit();
}

void source_init(source* s) {
    input_init(&s->input);
    decoder_init(&s->decoder);
    filter_init(&s->filter);
    taglist_init(&s->tagcache);

    s->tag_handler.cb = source_default_tag_handler;
    s->tag_handler.userdata = s;

    s->frame_destination = frame_receiver_zero;

    s->configuring = CONFIGURING_UNKNOWN;
}

void source_free(source* s) {
    input_free(&s->input);
    decoder_free(&s->decoder);
    filter_free(&s->filter);
    taglist_free(&s->tagcache);
}

int source_config(source* s, const strbuf* key, const strbuf* val) {
    int r;
    strbuf t = STRBUF_ZERO;

    if(strbuf_equals_cstr(key,"input")) {
        if( (r = input_create(&s->input,val)) != 0) {
            fprintf(stderr,"[source] error creating input\n");
            return r;
        }
        s->configuring = CONFIGURING_INPUT;
        return 0;
    }

    if(strbuf_equals_cstr(key,"decoder")) {
        if( (r = decoder_create(&s->decoder,val)) != 0) {
            fprintf(stderr,"[source] error creating decoder\n");
            return r;
        }
        s->configuring = CONFIGURING_DECODER;
        return 0;
    }

    if(strbuf_equals_cstr(key,"filter")) {
        if( (r = filter_create(&s->filter,val)) != 0) {
            fprintf(stderr,"[source] error creating filter\n");
            return r;
        }
        s->configuring = CONFIGURING_FILTER;
        return 0;
    }

    if(strbuf_begins_cstr(key,"input-")) {
        t.x = &key->x[6];
        t.len = key->len - 6;
        return input_config(&s->input,&t,val);
    }

    if(strbuf_begins_cstr(key,"decoder-")) {
        t.x = &key->x[8];
        t.len = key->len - 8;
        return decoder_config(&s->decoder,&t,val);
    }

    if(strbuf_begins_cstr(key,"filter-")) {
        t.x = &key->x[7];
        t.len = key->len - 7;
        return filter_config(&s->filter,&t,val);
    }

    switch(s->configuring) {
        case CONFIGURING_INPUT: return input_config(&s->input,key,val);
        case CONFIGURING_DECODER: return decoder_config(&s->decoder,key,val);
        case CONFIGURING_FILTER: return filter_config(&s->filter,key,val);
        case CONFIGURING_UNKNOWN: /* fall-through */
        default: break;
    }

    fprintf(stderr,"[source] unknown configuration option %.*s\n",(int)key->len,(const char *)key->x);
    return -1;
}

int source_open(source* s) {
    int r;

    if(s->filter.plugin == NULL) {
        if( (r = filter_create(&s->filter, &DEFAULT_FILTER)) != 0) {
            fprintf(stderr,"[source] unable to create filter plugin\n");
            return r;
        }
    }

    /* setup data forwards:
     *   input tags -> source
     *   decoder tags -> source
     *   decoder frames -> filter
     *   filter frames -> source */

    s->filter.frame_receiver.open = (frame_receiver_open_cb) source_open_intercept;
    s->filter.frame_receiver.submit_frame = (frame_receiver_submit_frame_cb) source_submit_frame_wrapper;
    s->filter.frame_receiver.flush = (frame_receiver_flush_cb) source_flush_wrapper;
    s->filter.frame_receiver.handle = s;

    s->decoder.frame_receiver.open = (frame_receiver_open_cb) filter_open;
    s->decoder.frame_receiver.submit_frame = (frame_receiver_submit_frame_cb) filter_submit_frame;
    s->decoder.frame_receiver.flush        = (frame_receiver_flush_cb) filter_flush;
    s->decoder.frame_receiver.handle       = &s->filter;

    s->decoder.tag_handler.cb       = source_tag_handler_wrapper;
    s->decoder.tag_handler.userdata = s;

    s->input.tag_handler.cb = source_tag_handler_wrapper;
    s->input.tag_handler.userdata = s;

    if( (r = input_open(&s->input)) != 0) return r;
    if( (r = decoder_open(&s->decoder,&s->input)) != 0) return r;

    return 0;
}

int source_set_tag_handler(source* s, const tag_handler* thandler) {
    memcpy(&s->tag_handler,thandler,sizeof(tag_handler));
    return 0;
}

int source_run(source* s) {
    int r;

    if(taglist_len(&s->tagcache) > 0) {
        if( (r = s->tag_handler.cb(s->tag_handler.userdata,&s->tagcache)) != 0) return r;
    }

    do {
        r = decoder_run(&s->decoder);
    } while(r == 0);
    return r != 1;
}

void source_dump_counters(const source* s, const strbuf* prefix) {
    input_dump_counters(&s->input,prefix);
    decoder_dump_counters(&s->decoder,prefix);
    filter_dump_counters(&s->filter,prefix);
}
