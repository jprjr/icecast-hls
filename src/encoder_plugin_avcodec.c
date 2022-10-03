#include "encoder_plugin.h"

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>

#include <stdlib.h>

struct plugin_userdata {
    const AVCodec* codec;
    AVCodecContext* ctx;
    AVFrame* avframe;
    AVPacket* avpacket;
    packet packet;
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
    userdata->codec = NULL;
    userdata->ctx = NULL;
    userdata->avframe = NULL;
    userdata->avpacket = NULL;
    packet_init(&userdata->packet);
    return userdata;
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    if(userdata->avframe != NULL) av_frame_free(&userdata->avframe);
    if(userdata->avpacket != NULL) av_packet_free(&userdata->avpacket);
    if(userdata->ctx != NULL) avcodec_free_context(&userdata->ctx);
    packet_free(&packet);
    return;
}

const encoder_plugin encoder_plugin_avcodec = {
    { .a = 0, .len = 7, .x = (uint8_t*)"avcodec" },
    plugin_init,
    plugin_deinit,
    plugin_create,
    NULL,
    NULL,
    plugin_close,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

