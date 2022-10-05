#include "filter_plugin_buffer.h"

#include <stdlib.h>
#include <stdio.h>

/* this just buffers audio */

struct plugin_userdata {
    unsigned int frame_len;
    frame buffer;
    frame out;
};

typedef struct plugin_userdata plugin_userdata;

static int plugin_init(void) {
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static void* plugin_create(void) {
    plugin_userdata* userdata = (plugin_userdata*)malloc(sizeof(plugin_userdata));
    if(userdata == NULL) return NULL;

    userdata->frame_len = 0;
    frame_init(&userdata->buffer);
    frame_init(&userdata->out);
    return userdata;
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    frame_free(&userdata->buffer);
    frame_free(&userdata->out);
    free(userdata);
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* val) {
    (void)ud;
    (void)key;
    (void)val;
    return 0;
}

static int plugin_handle_frame_source_params(void* ud, const frame_source_params* info) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    userdata->frame_len       = info->duration;

    if(info->format != SAMPLEFMT_UNKNOWN) {
        userdata->buffer.format = info->format;
        userdata->out.format    = info->format;
    }

    return 0;
}

static int plugin_open(void* ud, const frame_source* source, const frame_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;
    frame_source me = FRAME_SOURCE_ZERO;
    frame_source_params params = FRAME_SOURCE_PARAMS_ZERO;

    userdata->buffer.channels = source->channels;
    userdata->buffer.format   = source->format;
    userdata->buffer.sample_rate   = source->sample_rate;

    userdata->out.channels = source->channels;
    userdata->out.format   = source->format;
    userdata->out.sample_rate   = source->sample_rate;

    me = *source;
    me.handle = userdata;
    me.set_params = plugin_handle_frame_source_params;

    if( (r = dest->open(dest->handle, &me)) != 0) return r;

    if( (r = frame_ready(&userdata->buffer)) != 0) return r;
    if( (r = frame_ready(&userdata->out)) != 0) return r;

    params.format = source->format;

    return source->set_params(source->handle, &params);

}

static int plugin_drain_buffer(plugin_userdata* userdata, const frame_receiver* dest) {
    int r;
    size_t len;

    len = userdata->frame_len ? userdata->frame_len : userdata->buffer.duration;
    if(len == 0) return 0; /* no more audio frames buffered */

    while(userdata->buffer.duration >= len) {
        if( (r = frame_move(&userdata->out, &userdata->buffer, len)) != 0) return r;
        if( (r = dest->submit_frame(dest->handle,&userdata->out)) != 0) return r;
    }

    return 0;
}

static int plugin_submit_frame(void* ud, const frame* f, const frame_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;

    if( (r = frame_append(&userdata->buffer,f)) != 0) {
        fprintf(stderr,"[filter:buffer] error appending frame data\n");
        return r;
    }

    return plugin_drain_buffer(userdata,dest);
}

static int plugin_flush(void* ud, const frame_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;

    if( (r = plugin_drain_buffer(userdata,dest)) != 0) return r;

    if(userdata->buffer.duration > 0) {
        if( (r = frame_move(&userdata->out, &userdata->buffer, userdata->buffer.duration)) != 0) return r;
        if( (r = dest->submit_frame(dest->handle,&userdata->out)) != 0) return r;
    }

    return dest->flush(dest->handle);
}

const filter_plugin filter_plugin_buffer = {
    { .a = 0, .len = 6, .x = (uint8_t*)"buffer" },
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_submit_frame,
    plugin_flush,
};

