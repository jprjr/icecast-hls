#include "muxer_plugin_adts.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>

#include "adts_mux.h"

#define LOG_PREFIX "[muxer:adts]"
#include "logger.h"

#define LOGERRNO(s) log_error(s": %s", strerror(errno))

static STRBUF_CONST(plugin_name,"adts");
static STRBUF_CONST(mime_aac,"audio/aac");
static STRBUF_CONST(ext_aac,".aac");

struct muxer_plugin_adts_userdata {
    adts_mux adts_muxer;
};
typedef struct muxer_plugin_adts_userdata muxer_plugin_adts_userdata;

static size_t muxer_plugin_adts_size(void) {
    return sizeof(muxer_plugin_adts_userdata);
}

static int muxer_plugin_adts_reset(void* ud) {
    muxer_plugin_adts_userdata* userdata = (muxer_plugin_adts_userdata*)ud;

    adts_mux_init(&userdata->adts_muxer);

    return 0;
}

static int muxer_plugin_adts_create(void* ud) {
    muxer_plugin_adts_userdata* userdata = (muxer_plugin_adts_userdata*)ud;

    muxer_plugin_adts_reset(userdata);

    return 0;
}

static void muxer_plugin_adts_close(void* ud) {
    (void)ud;
}

static int muxer_plugin_adts_open(void* ud, const packet_source* source, const segment_receiver* dest) {
    muxer_plugin_adts_userdata* userdata = (muxer_plugin_adts_userdata*)ud;

    segment_source me = SEGMENT_SOURCE_ZERO;

    unsigned int sample_rate = source->sample_rate;
    unsigned int profile = source->profile;
    uint64_t channel_layout = source->channel_layout;

    switch(source->codec) {
        case CODEC_TYPE_AAC: {
            adts_mux_init(&userdata->adts_muxer);
            switch(profile) {
                case CODEC_PROFILE_AAC_LC: break;
                case CODEC_PROFILE_AAC_HE2: {
                    if(source->channel_layout != LAYOUT_STEREO) {
                        log_error("unsupported channels for HE2: requires stereo, total channels=%u",
                          (unsigned int)channel_count(source->channel_layout));
                        return -1;
                    }
                    channel_layout = LAYOUT_MONO;
                }
                /* fall-through */
                case CODEC_PROFILE_AAC_HE: sample_rate /= 2; profile = CODEC_PROFILE_AAC_LC; break;
                case CODEC_PROFILE_AAC_USAC: /* fall-through */
                default: {
                    log_error("unsupported AAC profile %u",source->profile);
                    return -1;
                }
            }

            if(adts_mux_set_sample_rate(&userdata->adts_muxer, sample_rate) != 0) {
                log_error("unsupported sample rate %u", sample_rate);
                return -1;
            }

            if(adts_mux_set_channel_layout(&userdata->adts_muxer, channel_layout) != 0) {
                log_error("unsupported channel layout 0x%" PRIx64, channel_layout);
                return -1;
            }
            adts_mux_set_profile(&userdata->adts_muxer, profile);

            me.media_ext = &ext_aac;
            me.media_mimetype = &mime_aac;
            break;
        }

        default: {
            log_error("unsupported codec %s", codec_name(source->codec));
            return -1;
        }
    }

    me.time_base = source->sample_rate;
    me.frame_len = source->frame_len;
    me.handle = userdata;

    return dest->open(dest->handle, &me);
}

static int muxer_plugin_adts_submit_packet(void* ud, const packet* packet, const segment_receiver* dest) {
    segment s;

    muxer_plugin_adts_userdata* userdata = (muxer_plugin_adts_userdata*)ud;

    adts_mux_encode_packet(&userdata->adts_muxer, packet->data.x, packet->data.len);

    s.type = SEGMENT_TYPE_MEDIA;
    s.data = userdata->adts_muxer.buffer;
    s.len  = userdata->adts_muxer.len;
    s.samples = packet->duration;
    s.pts = packet->pts;

    return dest->submit_segment(dest->handle,&s);
}

static int muxer_plugin_adts_flush(void* ud, const segment_receiver* dest) {
    (void)ud;
    (void)dest;
    return 0;
}

static int muxer_plugin_adts_submit_tags(void* ud, const taglist* tags, const segment_receiver* dest) {
    (void)ud;

    return dest->submit_tags(dest->handle,tags);
}

static int muxer_plugin_adts_init(void) {
    return 0;
}

static void muxer_plugin_adts_deinit(void) {
    return;
}

static int muxer_plugin_adts_config(void* ud, const strbuf* key, const strbuf* val) {
    (void)ud;
    (void)key;
    (void)val;
    return 0;
}

static uint32_t muxer_plugin_adts_get_caps(void* ud) {
    (void)ud;
    return 0;
}

static int muxer_plugin_adts_get_segment_info(const void* ud, const packet_source_info* s, const segment_receiver* dest, packet_source_params* i) {
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

const muxer_plugin muxer_plugin_adts = {
    plugin_name,
    muxer_plugin_adts_size,
    muxer_plugin_adts_init,
    muxer_plugin_adts_deinit,
    muxer_plugin_adts_create,
    muxer_plugin_adts_config,
    muxer_plugin_adts_open,
    muxer_plugin_adts_close,
    muxer_plugin_adts_submit_packet,
    muxer_plugin_adts_submit_tags,
    muxer_plugin_adts_flush,
    muxer_plugin_adts_reset,
    muxer_plugin_adts_get_caps,
    muxer_plugin_adts_get_segment_info,
};

