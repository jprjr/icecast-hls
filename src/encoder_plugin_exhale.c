#include "encoder_plugin.h"

#include <exhaleDecl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define KEY(v,t) static STRBUF_CONST(KEY_##v,#t)

KEY(vbr,vbr);
KEY(sbr,sbr);
KEY(use_noise_filling,use-noise-filling);

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

    int32_t* samples;
    frame buffer;

    unsigned int packetno;
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
};

typedef struct plugin_userdata plugin_userdata;

static int plugin_init(void) {
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* value) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(strbuf_equals(key,&KEY_vbr)) {
        userdata->vbr = strbuf_strtoul(value,10);
        if(errno != 0) {
            fprintf(stderr,"[encoder:exhale] error parsing vbr value %.*s\n",
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
        fprintf(stderr,"[encoder:exhale] error parsing sbr value: %.*s\n",
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
        fprintf(stderr,"[encoder:exhale] error parsing use-noise-filling value: %.*s\n",
          (int)value->len,(char *)value->x);
        return -1;
    }

    return 0;
}


static void* plugin_create(void) {
    plugin_userdata* userdata = (plugin_userdata*)malloc(sizeof(plugin_userdata));
    if(userdata == NULL) return NULL;

    userdata->exhale = NULL;
    packet_init(&userdata->packet);
    frame_init(&userdata->buffer);
    userdata->samples = NULL;
    userdata->packetno = 0;
    userdata->vbr = 3;
    userdata->frame_len = 1024;
    userdata->noise_filling = 1;
    userdata->tune_in_period = 0;
    userdata->discard_packets = 2;
    return userdata;
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    if(userdata->samples != NULL) free(userdata->samples);
    if(userdata->exhale != NULL) exhaleDelete(userdata->exhale);
    packet_free(&userdata->packet);
    frame_free(&userdata->buffer);
    free(userdata);
}

static int plugin_handle_packet_source_params(void* ud, const packet_source_params* params) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    userdata->tune_in_period = params->packets_per_segment;
    return 0;
}

static int plugin_open(void* ud, const frame_source* source, const packet_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    int r;
    uint8_t usac_config[16];
    uint32_t usac_config_size = 0;
    unsigned int padding = 0;
    membuf dsi = STRBUF_ZERO;
    packet_source me = PACKET_SOURCE_ZERO;
    memset(usac_config,0,sizeof(usac_config));

    if( (r = check_sample_rate(source->sample_rate)) < 0) {
        fprintf(stderr,"[encoder:exhale] unsupported sample rate %u\n",source->sample_rate);
        return r;
    }

    userdata->buffer.format = SAMPLEFMT_S32;
    userdata->buffer.channels = source->channels;
    userdata->buffer.duration = 0;
    userdata->buffer.sample_rate = source->sample_rate;

    if( (r = frame_ready(&userdata->buffer)) != 0) return r;

    me.codec       = CODEC_TYPE_AAC;
    me.profile     = CODEC_PROFILE_AAC_USAC;
    me.channels    = source->channels;
    me.sample_rate = source->sample_rate;
    me.frame_len   = userdata->frame_len;
    me.padding     = userdata->frame_len == 2048 ? 2048 : 0;
    me.sync_flag   = 0;
    me.roll_distance = userdata->frame_len == 2048 ? 2 : 1;
    me.roll_type   = 1;
    me.handle      = userdata;
    me.set_params  = plugin_handle_packet_source_params;

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

    if(( r = dest->open(dest->handle, &me)) != 0) {
        fprintf(stderr,"[encoder:exhale] error opening muxer\n");
        return r;
    }

    if(userdata->tune_in_period == 0) {
        /* default to a 1-second tune in period */
        userdata->tune_in_period = ((uint64_t)source->sample_rate) / ((uint64_t)userdata->frame_len);
    }

    userdata->samples = (int32_t*)malloc(sizeof(int32_t) * userdata->frame_len * source->channels);
    if(userdata->samples == NULL) return -1;
    memset(userdata->samples,0,sizeof(int32_t)*userdata->frame_len*source->channels);

    if( (r = membuf_ready(&userdata->packet.data,sizeof(uint8_t) * (9216/8) * source->channels)) != 0)
        return r;
    memset(userdata->packet.data.x,0,sizeof(uint8_t) * (9216/8) * source->channels);

    userdata->exhale = exhaleCreate(userdata->samples,userdata->packet.data.x,
      source->sample_rate, source->channels, userdata->frame_len,
      userdata->tune_in_period, userdata->vbr, userdata->noise_filling, 0);
    if(userdata->exhale == NULL) return -1;

    /* the exhale app seems to signal to the encoder to perform pre-rolling by setting
     * the first byte of usac_config to 1 before initializing the encoder? */
    usac_config[0] = 1;
    if(exhaleInitEncoder(userdata->exhale,usac_config,&usac_config_size) != 0) return -1;

    dsi.x = usac_config;
    dsi.len = usac_config_size;
    dsi.a = 0;

    if( (r = dest->submit_dsi(dest->handle,&dsi)) != 0) return r;

    userdata->packet.sample_rate = source->sample_rate;

    return 0;
}

static int plugin_encode_frame(plugin_userdata *userdata, const packet_receiver* dest) {
    int r = 0;
    unsigned int i = 0;
    unsigned int len;
    int32_t *samples;

    samples = (int32_t*)frame_get_channel_samples(&userdata->buffer,0);

    for(i=0;i< userdata->frame_len * userdata->buffer.channels ;i++) {
        userdata->samples[i] = samples[i] / ( 1 << 8 );
    }

    for( ; i < userdata->frame_len * userdata->buffer.channels ; i++) {
        userdata->samples[i] = 0;
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
    userdata->packet.duration = userdata->frame_len;
    userdata->packet.sync = userdata->packetno++ == 0;

    if(userdata->packetno == userdata->tune_in_period) userdata->packetno = 0;

    if( ( r = dest->submit_packet(dest->handle, &userdata->packet)) != 0) {
        fprintf(stderr,"[encoder:exhale] error sending packet to muxer\n");
        return r;
    }

    userdata->packet.pts += userdata->frame_len;
    return r;
}


static int plugin_drain(plugin_userdata* userdata, const packet_receiver* dest) {
    int r = 0;

    while(userdata->buffer.duration >= userdata->frame_len) {
        if( (r = plugin_encode_frame(userdata,dest)) != 0) break;
    }
    return r;

}

static int plugin_submit_frame(void* ud, const frame* frame, const packet_receiver* dest) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if( (r = frame_append(&userdata->buffer,frame)) != 0) {
        fprintf(stderr,"[encoder:exhale] error appending frame to buffer: %d\n",r);
        return r;
    }

    return plugin_drain(userdata, dest);


}

static int plugin_flush(void* ud, const packet_receiver* dest) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(userdata->buffer.duration > 0) {
        if(userdata->buffer.duration < userdata->frame_len) {
            if( (r = frame_fill(&userdata->buffer,userdata->frame_len)) != 0) {
                return r;
            }
        }
        if( (r = plugin_drain(userdata, dest)) != 0) return r;
    }

    return dest->flush(dest->handle);
}

const encoder_plugin encoder_plugin_exhale = {
    { .a = 0, .len = 6, .x = (uint8_t*)"exhale" },
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_submit_frame,
    plugin_flush,
};
