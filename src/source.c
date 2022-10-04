#include "source.h"
#include "input.h"

#include <stdio.h>
#include <string.h>

#define CONFIGURING_UNKNOWN 0
#define CONFIGURING_INPUT 1
#define CONFIGURING_DECODER 2
#define CONFIGURING_FILTER 3

static strbuf DEFAULT_FILTER = { .a = 0, .len = 11, .x = (uint8_t*)"passthrough" };

static int source_default_frame_handler(void* ud, const frame *frame) {
    (void)ud;
    (void)frame;
    fprintf(stderr,"[source] frame handler not set\n");
    return -1;
}

static int source_default_flush_handler(void* ud) {
    (void)ud;
    fprintf(stderr,"[source] flush handler not set\n");
    return -1;
}

static int source_default_tag_handler(void* ud, const taglist* tags) {
    source *s = (source *)ud;
    return taglist_deep_copy(&s->tagcache,tags);
}

static int source_default_audioconfig_handler(void* ud, const audioconfig* config) {
    source *s = (source *)ud;
    encoderinfo einfo = ENCODERINFO_ZERO;
    s->aconfig = *config;
    return config->info.submit(config->info.userdata, &einfo);
}

/* wrappers to forward calls from the input/decoder to whatever the
 * source is configured to use */
static int source_tag_handler_wrapper(void* ud, const taglist* tags) {
    source *s = (source *)ud;
    return s->tag_handler.cb(s->tag_handler.userdata, tags);
}

static int source_frame_handler_wrapper(void* ud, const frame* frame) {
    source *s = (source *)ud;
    return s->frame_handler.cb(s->frame_handler.userdata, frame);
}

static int source_flush_handler_wrapper(void* ud) {
    source *s = (source *)ud;
    return s->frame_handler.flush(s->frame_handler.userdata);
}

int source_open_dest(const source* s, const audioconfig_handler* handler) {
    audioconfig config = s->aconfig;
    config.info = encoderinfo_ignore;
    return handler->open(handler->userdata,&config);
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

    s->frame_handler.cb = source_default_frame_handler;
    s->frame_handler.flush = source_default_flush_handler;
    s->frame_handler.userdata = NULL;

    s->configuring = CONFIGURING_UNKNOWN;
}

void source_free(source* s) {
    input_free(&s->input);
    decoder_free(&s->decoder);
    taglist_free(&s->tagcache);
}

int source_config(source* s, const strbuf* key, const strbuf* val) {
    int r;

    if(strbuf_equals_cstr(key,"input")) {
        if( (r = input_create(&s->input,val)) != 0) return r;
        s->configuring = CONFIGURING_INPUT;
        return 0;
    }

    if(strbuf_equals_cstr(key,"decoder")) {
        if( (r = decoder_create(&s->decoder,val)) != 0) return r;
        s->configuring = CONFIGURING_DECODER;
        return 0;
    }

    if(strbuf_equals_cstr(key,"filter")) {
        if( (r = filter_create(&s->filter,val)) != 0) return r;
        s->configuring = CONFIGURING_FILTER;
        return 0;
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

    audioconfig_handler ahdlr_filter;
    audioconfig_handler ahdlr_decoder;

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

    s->filter.frame_handler.cb    = (frame_handler_callback) source_frame_handler_wrapper;
    s->filter.frame_handler.flush = (frame_handler_flush_callback) source_flush_handler_wrapper;
    s->filter.frame_handler.userdata = s;

    s->decoder.frame_handler.cb    = (frame_handler_callback) filter_submit_frame;
    s->decoder.frame_handler.flush = (frame_handler_flush_callback) filter_flush;
    s->decoder.frame_handler.userdata = &s->filter;

    s->decoder.tag_handler.cb       = source_tag_handler_wrapper;
    s->decoder.tag_handler.userdata = s;

    s->input.tag_handler.cb = source_tag_handler_wrapper;
    s->input.tag_handler.userdata = s;

    /* setup the config forwarders */
    ahdlr_filter.userdata = s;
    ahdlr_filter.open = source_default_audioconfig_handler;
    filter_set_audioconfig_handler(&s->filter, &ahdlr_filter);

    ahdlr_decoder.userdata = &s->filter;
    ahdlr_decoder.open     = (audioconfig_open_callback)filter_open;

    if( (r = input_open(&s->input)) != 0) return r;
    if( (r = decoder_open(&s->decoder,&s->input, &ahdlr_decoder)) != 0) return r;

    return 0;
}

int source_set_tag_handler(source* s, const tag_handler* thandler) {
    memcpy(&s->tag_handler,thandler,sizeof(tag_handler));
    return 0;
}

int source_set_frame_handler(source* s, const frame_handler* fhandler) {
    memcpy(&s->frame_handler,fhandler,sizeof(frame_handler));
    return 0;
}

int source_run(const source* s) {
    int r;

    if(taglist_len(&s->tagcache) > 0) {
        if( (r = s->tag_handler.cb(s->tag_handler.userdata,&s->tagcache)) != 0) return r;
    }

    return decoder_decode(&s->decoder);
}
