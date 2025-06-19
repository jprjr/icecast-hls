#include "encoder_plugin.h"

#include <exhaleDecl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

#include "mpeg_mappings.h"

#define KEY(v,t) static STRBUF_CONST(KEY_##v,#t)

#define LOG_PREFIX "[encoder:exhale]"
#include "logger.h"

KEY(vbr,vbr);
KEY(sbr,sbr);
KEY(use_noise_filling,use-noise-filling);

static STRBUF_CONST(plugin_name,"exhale");

static int check_sample_rate(unsigned int sample_rate) {
    switch(sample_rate) {
        case 96000: /* fall-through */
        case 88200: /* fall-through */
        case 64000: /* fall-through */
        case 48000: /* fall-through */
        case 44100: /* fall-through */
        case 32000: /* fall-through */
        case 24000: /* fall-through */
        case 22050: /* fall-through */
        case 16000: /* fall-through */
        case 12000: /* fall-through */
        case 11025: /* fall-through */
        case  8000: /* fall-through */
        case  7350: {
            return 0;
        }
        default: break;
    }
    return -1;
}

struct plugin_userdata {
    ExhaleEncAPI *exhale;
    packet packet;

    frame buffer;
    frame samples;

    unsigned int frame_len;
    unsigned int tune_in_period;

    /* configurable things */
    uint8_t vbr;
    uint8_t noise_filling;

    /* this isn't really documented in the exhale wiki, but
     * you need to pad the beginning of the input with
     * 0s and discard the first two packets. The first
     * packet needs to be encoded with exhaleEncodeLookahead(). */
    unsigned int discard_packets;
    packet_source me;
};

typedef struct plugin_userdata plugin_userdata;

static int plugin_init(void) {
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static size_t plugin_size(void) {
    return sizeof(plugin_userdata);
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* value) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(strbuf_equals(key,&KEY_vbr)) {
        userdata->vbr = strbuf_strtoul(value,10);
        if(errno != 0) {
            log_error("error parsing vbr value %.*s",
              (int)value->len,(char *)value->x);
            return -1;
        }
        return 0;
    }

    if(strbuf_equals(key,&KEY_sbr)) {
        if(strbuf_truthy(value)) {
            userdata->frame_len = 2048;
            return 0;
        }
        if(strbuf_falsey(value)) {
            userdata->frame_len = 1024;
            return 0;
        }
        log_error("error parsing sbr value: %.*s",
          (int)value->len,(char *)value->x);
        return -1;
    }

    if(strbuf_equals(key,&KEY_use_noise_filling)) {
        if(strbuf_truthy(value)) {
            userdata->noise_filling = 1;
            return 0;
        }
        if(strbuf_falsey(value)) {
            userdata->noise_filling = 0;
            return 0;
        }
        log_error("error parsing use-noise-filling value: %.*s",
          (int)value->len,(char *)value->x);
        return -1;
    }

    return 0;
}


static int plugin_create(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    userdata->exhale = NULL;
    packet_init(&userdata->packet);
    frame_init(&userdata->buffer);
    frame_init(&userdata->samples);
    userdata->vbr = 3;
    userdata->frame_len = 1024;
    userdata->noise_filling = 1;
    userdata->tune_in_period = 0;
    userdata->discard_packets = 2;
    userdata->me = packet_source_zero;

    return 0;
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(userdata->exhale != NULL) exhaleDelete(userdata->exhale);
    packet_free(&userdata->packet);
    frame_free(&userdata->buffer);
    frame_free(&userdata->samples);
    packet_source_free(&userdata->me);
}

static int plugin_open(void* ud, const frame_source* source, const packet_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    int r;
    uint8_t usac_config[16];
    uint32_t usac_config_size = 0;
    unsigned int padding = 0;

    packet_source_info ps_info = PACKET_SOURCE_INFO_ZERO;
    packet_source_params ps_params = PACKET_SOURCE_PARAMS_ZERO;
    memset(usac_config,0,sizeof(usac_config));

    ps_info.time_base = source->sample_rate;
    ps_info.frame_len = userdata->frame_len;

    if( (r = check_sample_rate(source->sample_rate)) < 0) {
        log_error("unsupported sample rate %u",source->sample_rate);
        return r;
    }

    switch(source->channel_layout) {
        case LAYOUT_MONO: /* fall-through */
        case LAYOUT_STEREO: /* fall-through */
        case LAYOUT_3_0: /* fall-through */
        case LAYOUT_4_0: /* fall-through */
        case LAYOUT_5_0: /* fall-through */
        case LAYOUT_5_1: break;
        default: {
            log_error("unsupported channel layout 0x%" PRIx64,source->channel_layout);
            return -1;
        }
    }

    if( (r = dest->get_segment_info(dest->handle, &ps_info, &ps_params)) != 0) {
        logs_error("error getting segment info");
        return r;
    }

    if(ps_params.packets_per_subsegment == 0) ps_params.packets_per_subsegment = ps_params.packets_per_segment;

    logs_debug("opening");
    log_debug("  vbr = %u", userdata->vbr);
    log_debug("  sbr = %u", userdata->frame_len == 2048);
    log_debug("  samplerate = %u", source->sample_rate);
    log_debug("  channel layout = 0x%" PRIx64, source->channel_layout);
    log_debug("  packets_per_segment = %u", ps_params.packets_per_segment);
    log_debug("  packets_per_subsegment = %u", ps_params.packets_per_subsegment);

    /* it seems like exhale's tune-in period is really double what you intend.
     * I think the idea goes the tune_in_period allows you to technically start
     * decoding every (x) packets but what we really want are true sync packets */

    /* TODO Apple guidelines regarding segmentation state:
     *   "7.8. Each xHE-AAC segment SHOULD start with an Immediate Playout Frame (IPF)."
     *   Unclear if this applies to partial segments as well or stricty segments.
     *
     *   Would prefer variable segment sizes, but a fixed tune-in period
     *   means we can't guarantee every  segment will start on an IPF.
     *
     *   So, we'll just have fixed segment sizes. */
    userdata->tune_in_period = ps_params.packets_per_segment >> 1;
    if(!userdata->tune_in_period) userdata->tune_in_period = 1;
    log_debug("  tune_in_period = %u", userdata->tune_in_period);

    userdata->buffer.format = SAMPLEFMT_S32P;
    userdata->buffer.channels = channel_count(source->channel_layout);
    userdata->buffer.duration = 0;
    userdata->buffer.sample_rate = source->sample_rate;

    userdata->samples.format = SAMPLEFMT_S32;
    userdata->samples.channels = channel_count(source->channel_layout);
    userdata->samples.duration = userdata->frame_len;
    userdata->samples.sample_rate = source->sample_rate;

    userdata->packet.sample_rate = source->sample_rate;

    if( (r = frame_ready(&userdata->buffer)) != 0) return r;
    if( (r = frame_buffer(&userdata->samples)) != 0) return r;

    userdata->me.codec       = CODEC_TYPE_AAC;
    userdata->me.profile     = CODEC_PROFILE_AAC_USAC;
    userdata->me.channel_layout    = source->channel_layout;
    userdata->me.sample_rate = source->sample_rate;
    userdata->me.frame_len   = userdata->frame_len;
    userdata->me.padding     = userdata->frame_len == 2048 ? 2048 : 0;
    userdata->me.roll_distance = userdata->frame_len == 2048 ? 2 : 1;
    userdata->me.roll_type   = 1;
    userdata->me.handle      = userdata;
    userdata->me.sync_flag   = 0;


    /* simplifying some logic from the exhale app:
     * startLength = 1600 / 3200 (regular vs sbr)
     * resampRatio = 1 (no resampling)
     * resampShift = 0 (no resampling)
     * sbrEncDelay = 962 (only with SBR)
     * firstLength = frameLen
     * the formula is something like:
     *  padding = (userdata->frame_len << 1) - (userdata->frame_len == 1024 ? 1600: 3200) +
     *            (userdata->frame_len == 1024 ? 0 : userdata->frame_len - 962);
     * this means it's either:
     *   non-sbr: 448
     *       sbr: 1982
     */
    padding = (userdata->frame_len == 1024 ? 448 : 1982);

    if( (r = frame_fill(&userdata->buffer,padding)) != 0) return r;

    if(userdata->tune_in_period == 0) {
        /* default to a 1-second tune in period */
        userdata->tune_in_period = ((uint64_t)source->sample_rate) / ((uint64_t)userdata->frame_len);
    }

    if( (r = membuf_ready(&userdata->packet.data,sizeof(uint8_t) * (9216/8) * userdata->buffer.channels)) != 0)
        return r;
    memset(userdata->packet.data.x,0,sizeof(uint8_t) * (9216/8) * userdata->buffer.channels);

    userdata->exhale = exhaleCreate(frame_get_channel_samples(&userdata->samples,0),
      userdata->packet.data.x, source->sample_rate, userdata->buffer.channels,
      userdata->frame_len, userdata->tune_in_period, userdata->vbr, userdata->noise_filling, 0);
    if(userdata->exhale == NULL) return -1;

    /* the exhale app seems to signal to the encoder to perform pre-rolling by setting
     * the first byte of usac_config to 1 before initializing the encoder? */
    usac_config[0] = 1;
    if(exhaleInitEncoder(userdata->exhale,usac_config,&usac_config_size) != 0) return -1;

    userdata->me.dsi.x = usac_config;
    userdata->me.dsi.len = usac_config_size;
    userdata->me.dsi.a = 0;

    if(( r = dest->open(dest->handle, &userdata->me)) != 0) {
        logs_error("error opening muxer");
        return r;
    }

    return 0;
}

static int plugin_encode_frame(plugin_userdata *userdata, const packet_receiver* dest, unsigned int frame_len) {
    int r = 0;
    size_t i = 0;
    size_t j = 0;
    size_t c = 0;
    size_t len;
    int32_t *samples;
    int32_t *src;

    samples = (int32_t*)frame_get_channel_samples(&userdata->samples,0);

    for(i=0;i<userdata->buffer.channels;i++) {
        c = (size_t)mpeg_channel_layout[userdata->buffer.channels][i];

        src = frame_get_channel_samples(&userdata->buffer,i);
        for(j=0;j<userdata->frame_len;j++) {
            samples[(j * ((size_t)userdata->buffer.channels)) + c] = src[j] / (1 << 8);
        }
    }

    frame_trim(&userdata->buffer, userdata->frame_len);

    switch(userdata->discard_packets) {
        case 2: len = exhaleEncodeLookahead(userdata->exhale); break;
        default: len = exhaleEncodeFrame(userdata->exhale); break;
    }

    if(len < 3 || len == 65535) return -1;
    if(userdata->discard_packets) {
        userdata->discard_packets--;
        return 0;
    }

    userdata->packet.data.len = len;
    userdata->packet.duration = frame_len;
    userdata->packet.sync = (userdata->packet.data.x[0] & 0xC0) == 0xC0;
    userdata->packet.sample_group = (userdata->packet.data.x[0] & 0xC0) == 0x80 ? 1 : 0;

    if( ( r = dest->submit_packet(dest->handle, &userdata->packet)) != 0) {
        logs_error("error sending packet to muxer");
        return r;
    }

    userdata->packet.pts += userdata->frame_len;
    return r;
}


static int plugin_drain(plugin_userdata* userdata, const packet_receiver* dest) {
    int r = 0;

    while(userdata->buffer.duration >= userdata->frame_len) {
        if( (r = plugin_encode_frame(userdata,dest,userdata->frame_len)) != 0) break;
    }
    return r;

}

static int plugin_submit_frame(void* ud, const frame* frame, const packet_receiver* dest) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if( (r = frame_append(&userdata->buffer,frame)) != 0) {
        log_error("error appending frame to buffer: %d",r);
        return r;
    }

    return plugin_drain(userdata, dest);


}

static int plugin_flush(void* ud, const packet_receiver* dest) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;
    unsigned int duration = 0;

    /* first flush out any whole frames */
    if(userdata->buffer.duration > 0) {
        if( (r = plugin_drain(userdata, dest)) != 0) return r;
    }

    /* record the final partial duration */
    if(userdata->buffer.duration > 0) {
        duration = userdata->buffer.duration;
        if( (r = frame_fill(&userdata->buffer,userdata->frame_len)) != 0) {
            return r;
        }
        if( (r = plugin_drain(userdata, dest)) != 0) return r;
    }

    /* finally encode two final empty frames */
    if( (r = frame_fill(&userdata->buffer,userdata->frame_len)) != 0) return r;
    if( (r = plugin_encode_frame(userdata, dest, userdata->frame_len)) != 0) return r;

    if( (r = frame_fill(&userdata->buffer,userdata->frame_len)) != 0) return r;
    if( (r = plugin_encode_frame(userdata, dest, duration)) != 0) return r;

    return 0;
}

static int plugin_reset(void* ud) {
    (void)ud;
    logs_fatal("no reset function");
    abort();
    return -1;
}

const encoder_plugin encoder_plugin_exhale = {
    &plugin_name,
    plugin_size,
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_submit_frame,
    plugin_flush,
    plugin_reset,
};
