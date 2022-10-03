#include "source.h"
#include "input.h"

#include <stdio.h>
#include <string.h>

#define CONFIGURING_UNKNOWN 0
#define CONFIGURING_INPUT 1
#define CONFIGURING_DECODER 2

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
    return 0;
}

void source_global_deinit(void) {
    input_global_deinit();
    decoder_global_deinit();
}

void source_init(source* s) {
    input_init(&s->input);
    decoder_init(&s->decoder);
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

    switch(s->configuring) {
        case CONFIGURING_INPUT: return input_config(&s->input,key,val);
        case CONFIGURING_DECODER: return decoder_config(&s->decoder,key,val);
        case CONFIGURING_UNKNOWN: /* fall-through */
        default: break;
    }

    fprintf(stderr,"[source] unknown configuration option %.*s\n",(int)key->len,(const char *)key->x);
    return -1;
}

int source_open(source* s) {
    int r;

    frame_handler fhdlr;
    tag_handler thdlr;
    audioconfig_handler ahdlr;

    fhdlr.cb = source_frame_handler_wrapper;
    fhdlr.flush = source_flush_handler_wrapper;
    fhdlr.userdata = s;

    thdlr.cb = source_tag_handler_wrapper;
    thdlr.userdata = s;

    ahdlr.open = source_default_audioconfig_handler;
    ahdlr.userdata = s;

    input_set_tag_handler(&s->input, &thdlr);

    decoder_set_tag_handler(&s->decoder, &thdlr);
    decoder_set_frame_handler(&s->decoder, &fhdlr);

    if( (r = input_open(&s->input)) != 0) return r;
    if( (r = decoder_open(&s->decoder,&s->input, &ahdlr)) != 0) return r;

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
