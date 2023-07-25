#include "decoder_plugin_passthrough.h"

#include "strbuf.h"

#include <stdlib.h>

static STRBUF_CONST(plugin_name, "passthrough");

struct plugin_userdata {
    frame_source me;
    frame f;
};

typedef struct plugin_userdata plugin_userdata;

static int plugin_init(void) { return 0; }
static void plugin_deinit(void) { return; }
static size_t plugin_size(void) { return sizeof(plugin_userdata); }

static int plugin_create(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    frame_init(&userdata->f);
    userdata->me = frame_source_zero;
    membuf_init(&userdata->me.packet_source.dsi);

    return 0;
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    frame_free(&userdata->f);

}

static int plugin_open(void* ud, const packet_source* src, const frame_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;

    userdata->me.format = SAMPLEFMT_BINARY;
    userdata->me.channel_layout = src->channel_layout;
    userdata->me.duration = src->frame_len;
    userdata->me.sample_rate = src->sample_rate;

    if( (r = packet_source_copy(&userdata->me.packet_source, src)) != 0) return r;

    return dest->open(dest->handle,&userdata->me);
}

static int plugin_decode(void* ud, const packet* src, const frame_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;

    userdata->f.format = SAMPLEFMT_BINARY;
    userdata->f.channels = channel_count(userdata->me.channel_layout);
    userdata->f.duration = src->duration;
    userdata->f.sample_rate = src->sample_rate;
    userdata->f.pts = src->pts;

    if( (r = packet_copy(&userdata->f.packet,src)) != 0) return r;

    return dest->submit_frame(dest->handle,&userdata->f);
}

static int plugin_flush(void* ud, const frame_receiver* dest) {
    (void)ud;
    (void)dest;
    return 0;
}

static int plugin_reset(void* ud) {
    (void)ud;
    return 0;
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* val) {
    (void)ud;
    (void)key;
    (void)val;
    return 0;
}


const decoder_plugin decoder_plugin_passthrough = {
    plugin_name,
    plugin_size,
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_decode,
    plugin_flush,
    plugin_reset,
};
