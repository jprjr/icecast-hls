#include "muxer_plugin_passthrough.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define LOG0(str) fprintf(stderr,"[muxer:passthrough] "str"\n")
#define LOG1(s, a) fprintf(stderr,"[muxer:passthrough] "s"\n", (a))

#define LOGERRNO(s) LOG1(s": %s", strerror(errno))
#define TRY(exp, act) if(!(exp)) { act; }
#define TRYNULL(exp, act) if((exp) == NULL) { act; }

static STRBUF_CONST(plugin_name,"passthrough");

static STRBUF_CONST(mime_mp3,"audio/mpeg");
static STRBUF_CONST(mime_ac3,"audio/ac3");
static STRBUF_CONST(mime_eac3,"audio/eac3");

static STRBUF_CONST(ext_mp3,".mp3");
static STRBUF_CONST(ext_ac3,".ac3");
static STRBUF_CONST(ext_eac3,".eac3");

struct muxer_plugin_passthrough_userdata {
    uint8_t dummy;
};
typedef struct muxer_plugin_passthrough_userdata muxer_plugin_passthrough_userdata;

static size_t muxer_plugin_passthrough_size(void) {
    return sizeof(muxer_plugin_passthrough_userdata);
}

static int muxer_plugin_passthrough_create(void* ud) {
    (void)ud;

    return 0;
}

static void muxer_plugin_passthrough_close(void* ud) {
    (void)ud;
}

static int muxer_plugin_passthrough_open(void* ud, const packet_source* source, const segment_receiver* dest) {
    muxer_plugin_passthrough_userdata* userdata = (muxer_plugin_passthrough_userdata*)ud;

    segment_source me = SEGMENT_SOURCE_ZERO;

    switch(source->codec) {
        case CODEC_TYPE_MP3: {
            me.media_ext = &ext_mp3;
            me.media_mimetype = &mime_mp3;
            break;
        }

        case CODEC_TYPE_AC3: {
            me.media_ext = &ext_ac3;
            me.media_mimetype = &mime_ac3;
            break;
        }

        case CODEC_TYPE_EAC3: {
            me.media_ext = &ext_eac3;
            me.media_mimetype = &mime_eac3;
            break;
        }

        default: {
            LOG1("unsupported codec %s", codec_name(source->codec));
            return -1;
        }
    }

    me.time_base = source->sample_rate;
    me.frame_len = source->frame_len;
    me.handle = userdata;

    return dest->open(dest->handle, &me);
}

static int muxer_plugin_passthrough_submit_packet(void* ud, const packet* packet, const segment_receiver* dest) {
    segment s;

    (void)ud;

    s.type = SEGMENT_TYPE_MEDIA;
    s.data = packet->data.x;
    s.len  = packet->data.len;
    s.samples = packet->duration;
    s.pts = packet->pts;
    return dest->submit_segment(dest->handle,&s);
}

static int muxer_plugin_passthrough_flush(void* ud, const segment_receiver* dest) {
    (void)ud;
    (void)dest;
    return 0;
}

static int muxer_plugin_passthrough_reset(void* ud) {
    (void)ud;
    return 0;
}

static int muxer_plugin_passthrough_submit_tags(void* ud, const taglist* tags, const segment_receiver* dest) {
    (void)ud;

    return dest->submit_tags(dest->handle,tags);
}

static int muxer_plugin_passthrough_init(void) {
    return 0;
}

static void muxer_plugin_passthrough_deinit(void) {
    return;
}

static int muxer_plugin_passthrough_config(void* ud, const strbuf* key, const strbuf* val) {
    (void)ud;
    (void)key;
    (void)val;
    return 0;
}

static uint32_t muxer_plugin_passthrough_get_caps(void* ud) {
    (void)ud;
    return 0;
}

static int muxer_plugin_passthrough_get_segment_info(const void* ud, const packet_source_info* s, const segment_receiver* dest, packet_source_params* i) {
    (void)ud;

    segment_source_info s_info = SEGMENT_SOURCE_INFO_ZERO;
    segment_params s_params = SEGMENT_PARAMS_ZERO;

    s_info.time_base = s->time_base;
    s_info.frame_len = s->frame_len;

    dest->get_segment_info(dest->handle,&s_info,&s_params);
    i->segment_length = s_params.segment_length;
    i->packets_per_segment = s_params.packets_per_segment;
    return 0;
}

const muxer_plugin muxer_plugin_passthrough = {
    plugin_name,
    muxer_plugin_passthrough_size,
    muxer_plugin_passthrough_init,
    muxer_plugin_passthrough_deinit,
    muxer_plugin_passthrough_create,
    muxer_plugin_passthrough_config,
    muxer_plugin_passthrough_open,
    muxer_plugin_passthrough_close,
    muxer_plugin_passthrough_submit_packet,
    muxer_plugin_passthrough_submit_tags,
    muxer_plugin_passthrough_flush,
    muxer_plugin_passthrough_reset,
    muxer_plugin_passthrough_get_caps,
    muxer_plugin_passthrough_get_segment_info,
};

