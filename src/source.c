#include "source.h"
#include "input.h"

#include <stdio.h>
#include <string.h>

#define CONFIGURING_UNKNOWN 0
#define CONFIGURING_INPUT 1
#define CONFIGURING_DEMUXER 2
#define CONFIGURING_DECODER 3
#define CONFIGURING_FILTER 4

static STRBUF_CONST(DEFAULT_DEMUXER, "auto");
static STRBUF_CONST(DEFAULT_DECODER, "auto");
static STRBUF_CONST(DEFAULT_FILTER, "passthrough");

static int source_default_tag_handler(void* ud, const taglist* tags) {
    source *s = (source *)ud;
    return taglist_deep_copy(&s->tagcache,tags);
}

static int source_tag_handler_wrapper(void* ud, const taglist* tags) {
    source *s = (source *)ud;
    return s->tag_handler.cb(s->tag_handler.userdata, tags);
}

static int source_frame_receiver_open(void* ud, const frame_source* src) {
    source *s = (source *) ud;
    return s->frame_receiver.open(s->frame_receiver.handle, src);
}

static int source_frame_receiver_submit_frame(void* ud, const frame* src) {
    source *s = (source *) ud;
    return s->frame_receiver.submit_frame(s->frame_receiver.handle, src);
}

static int source_frame_receiver_flush(void* ud) {
    source *s = (source *) ud;
    return s->frame_receiver.flush(s->frame_receiver.handle);
}

static int source_frame_receiver_reset(void* ud) {
    source *s = (source *) ud;
    return s->frame_receiver.reset(s->frame_receiver.handle);
}

int source_global_init(void) {
    int r;
    if( (r = input_global_init()) != 0) return r;
    if( (r = demuxer_global_init()) != 0) return r;
    if( (r = decoder_global_init()) != 0) return r;
    if( (r = filter_global_init()) != 0) return r;
    return 0;
}

void source_global_deinit(void) {
    input_global_deinit();
    demuxer_global_deinit();
    decoder_global_deinit();
    filter_global_deinit();
}

void source_init(source* s) {
    input_init(&s->input);
    demuxer_init(&s->demuxer);
    decoder_init(&s->decoder);
    filter_init(&s->filter);
    taglist_init(&s->tagcache);

    s->tag_handler.cb = source_default_tag_handler;
    s->tag_handler.userdata = s;

    s->frame_receiver = frame_receiver_zero;

    s->configuring = CONFIGURING_UNKNOWN;
}

void source_free(source* s) {
    input_free(&s->input);
    demuxer_free(&s->demuxer);
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

    if(strbuf_equals_cstr(key,"demuxer")) {
        if( (r = demuxer_create(&s->demuxer,val)) != 0) {
            fprintf(stderr,"[source] error creating demuxer\n");
            return r;
        }
        s->configuring = CONFIGURING_DEMUXER;
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

    if(strbuf_begins_cstr(key,"demuxer-")) {
        t.x = &key->x[8];
        t.len = key->len - 8;
        return demuxer_config(&s->demuxer,&t,val);
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
        case CONFIGURING_DEMUXER: return demuxer_config(&s->demuxer,key,val);
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

    if(s->demuxer.plugin == NULL) {
        if( (r = demuxer_create(&s->demuxer, &DEFAULT_DEMUXER)) != 0) {
            fprintf(stderr,"[source] unable to create demuxer plugin\n");
            return r;
        }
    }

    if(s->decoder.plugin == NULL) {
        if( (r = decoder_create(&s->decoder, &DEFAULT_DECODER)) != 0) {
            fprintf(stderr,"[source] unable to create decoder plugin\n");
            return r;
        }
    }

    if(s->filter.plugin == NULL) {
        if( (r = filter_create(&s->filter, &DEFAULT_FILTER)) != 0) {
            fprintf(stderr,"[source] unable to create filter plugin\n");
            return r;
        }
    }

    /* setup data forwards:
     *   input tags -> source
     *   demuxer tags -> source
     *   demuxer packets -> decoder
     *   decoder frames -> filter
     *   filter frames -> source */

    s->demuxer.packet_receiver.open           = (packet_receiver_open_cb) decoder_open;
    s->demuxer.packet_receiver.submit_packet  = (packet_receiver_submit_packet_cb) decoder_submit_packet;
    s->demuxer.packet_receiver.flush          = (packet_receiver_flush_cb) decoder_flush;
    s->demuxer.packet_receiver.handle         = &s->decoder;

    s->decoder.frame_receiver.open = (frame_receiver_open_cb) filter_open;
    s->decoder.frame_receiver.submit_frame = (frame_receiver_submit_frame_cb) filter_submit_frame;
    s->decoder.frame_receiver.flush        = (frame_receiver_flush_cb) filter_flush;
    s->decoder.frame_receiver.reset        = (frame_receiver_reset_cb) filter_reset;
    s->decoder.frame_receiver.handle       = &s->filter;

    s->demuxer.tag_handler.cb       = source_tag_handler_wrapper;
    s->demuxer.tag_handler.userdata = s;

    s->input.tag_handler.cb = source_tag_handler_wrapper;
    s->input.tag_handler.userdata = s;

    s->filter.frame_receiver.handle = s;
    s->filter.frame_receiver.open = source_frame_receiver_open;
    s->filter.frame_receiver.submit_frame = source_frame_receiver_submit_frame;
    s->filter.frame_receiver.flush = source_frame_receiver_flush;
    s->filter.frame_receiver.reset = source_frame_receiver_reset;

    if( (r = input_open(&s->input)) != 0) return r;
    if( (r = demuxer_open(&s->demuxer,&s->input)) != 0) return r;

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

    tryagain:
    do {
        r = demuxer_run(&s->demuxer);
    } while(r == 0);

    if(r == 2) {
        if( (r = decoder_flush(&s->decoder)) != 0) goto done;
        if( (r = decoder_reset(&s->decoder)) != 0) goto done;

        /*
         * demuxer_run will call decoder_open, if decoder_open
         * detects a format change it will flush + reset + open the filter.
         * Similarly if filter_open detects a format change, it will
         * flush + reset + open the destination
         */

        goto tryagain;
    }

    done:
    return r != 1;
}

void source_dump_counters(const source* s, const strbuf* prefix) {
    input_dump_counters(&s->input,prefix);
    demuxer_dump_counters(&s->demuxer,prefix);
    decoder_dump_counters(&s->decoder,prefix);
    filter_dump_counters(&s->filter,prefix);
}
