#include "filter_plugin_passthrough.h"

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

static int plugin_handle_encoderinfo(void* ud, const encoderinfo* info) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    userdata->frame_len       = info->frame_len;

    if(info->format != SAMPLEFMT_UNKNOWN) {
        userdata->buffer.format      = info->format;
        userdata->out.format      = info->format;
    }

    return 0;
}

static int plugin_open(void* ud, const audioconfig* config, const audioconfig_handler* handler) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;
    audioconfig oconfig = AUDIOCONFIG_ZERO;
    encoderinfo info = ENCODERINFO_ZERO;

    userdata->buffer.channels = config->channels;
    userdata->buffer.format   = config->format;
    userdata->buffer.sample_rate   = config->sample_rate;

    userdata->out.channels = config->channels;
    userdata->out.format   = config->format;
    userdata->out.sample_rate   = config->sample_rate;

    oconfig = *config;
    oconfig.info.userdata = userdata;
    oconfig.info.submit = plugin_handle_encoderinfo;

    if( (r = handler->open(handler->userdata, &oconfig)) != 0) return r;

    if( (r = frame_ready(&userdata->buffer)) != 0) return r;
    if( (r = frame_ready(&userdata->out)) != 0) return r;

    info.format = config->format;

    return config->info.submit(config->info.userdata, &info);

}

static int plugin_drain_buffer(plugin_userdata* userdata, const frame_handler* handler) {
    int r;
    size_t len;

    len = userdata->frame_len ? userdata->frame_len : userdata->buffer.duration;

    while(userdata->buffer.duration >= len) {
        if( (r = frame_move(&userdata->out, &userdata->buffer, len)) != 0) return r;
        if( (r = handler->cb(handler->userdata,&userdata->out)) != 0) return r;
    }

    return 0;
}

static int plugin_submit_frame(void* ud, const frame* f, const frame_handler* handler) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;

    if( (r = frame_append(&userdata->buffer,f)) != 0) {
        fprintf(stderr,"[filter:passthrough] error appending frame data\n");
        return r;
    }

    return plugin_drain_buffer(userdata,handler);
}

static int plugin_flush(void* ud, const frame_handler* handler) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;

    if( (r = plugin_drain_buffer(userdata,handler)) != 0) return r;

    return handler->flush(handler->userdata);
}

const filter_plugin filter_plugin_passthrough = {
    { .a = 0, .len = 11, .x = (uint8_t*)"passthrough" },
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_submit_frame,
    plugin_flush,
};

