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

static void* muxer_plugin_passthrough_create(void) {
    muxer_plugin_passthrough_userdata* userdata = NULL;
    TRYNULL(userdata = (muxer_plugin_passthrough_userdata*)malloc(sizeof(muxer_plugin_passthrough_userdata)),
      LOGERRNO("error allocating plugin"); abort());

    return userdata;
}

static void muxer_plugin_passthrough_close(void* ud) {
    muxer_plugin_passthrough_userdata* userdata = (muxer_plugin_passthrough_userdata*)ud;
    free(userdata);
}

static int muxer_plugin_passthrough_open(void* ud, const packet_source* source, const segment_receiver* dest) {
    int r;
    muxer_plugin_passthrough_userdata* userdata = (muxer_plugin_passthrough_userdata*)ud;

    segment_source me = SEGMENT_SOURCE_ZERO;
    segment_source_info info = SEGMENT_SOURCE_INFO_ZERO;
    segment_params s_params = SEGMENT_PARAMS_ZERO;
    packet_source_params params = PACKET_SOURCE_PARAMS_ZERO;

    info.time_base = source->sample_rate;
    info.frame_len = source->frame_len;

    /* inform output of our time base and frame len, we'll just
     * discard that though */
    dest->get_segment_params(dest->handle,&info,&s_params);
    params.packets_per_segment = s_params.packets_per_segment;

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

#if 0
    me.time_base = source->sample_rate;
    me.frame_len = source->frame_len;
#endif
    me.handle = userdata;

    if( (r = dest->open(dest->handle, &me)) != 0) return r;
    return source->set_params(source->handle, &params);
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
    return dest->flush(dest->handle);
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

static int muxer_plugin_passthrough_submit_dsi(void* ud, const membuf* data,const segment_receiver* dest) {
    (void)ud;
    (void)data;
    (void)dest;
    return 0;
}

static int muxer_plugin_passthrough_config(void* ud, const strbuf* key, const strbuf* val) {
    (void)ud;
    (void)key;
    (void)val;
    return 0;
}

static int muxer_plugin_passthrough_get_caps(void* ud, packet_receiver_caps* caps) {
    (void)ud;
    caps->has_global_header = 0;
    return 0;
}

const muxer_plugin muxer_plugin_passthrough = {
    {.a = 0, .len = 11, .x = (uint8_t*)"passthrough" },
    muxer_plugin_passthrough_init,
    muxer_plugin_passthrough_deinit,
    muxer_plugin_passthrough_create,
    muxer_plugin_passthrough_config,
    muxer_plugin_passthrough_open,
    muxer_plugin_passthrough_close,
    muxer_plugin_passthrough_submit_dsi,
    muxer_plugin_passthrough_submit_packet,
    muxer_plugin_passthrough_submit_tags,
    muxer_plugin_passthrough_flush,
    muxer_plugin_passthrough_get_caps,
};

