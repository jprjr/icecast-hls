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

    unsigned int packetno;
    unsigned int frame_len;
    unsigned int tune_in_period;

    /* configurable things */
    uint8_t vbr;
    uint8_t noise_filling;

    /* exhale requires a separate function call for the first frame,
     * so I use a pointer that updates itself after the first call */
    unsigned int (*encode_func)(struct plugin_userdata*);
};

typedef struct plugin_userdata plugin_userdata;

static unsigned int encode_regular_frame(plugin_userdata *userdata) {
    return exhaleEncodeFrame(userdata->exhale);
}

static unsigned int encode_first_frame(plugin_userdata *userdata) {
    unsigned int r = exhaleEncodeLookahead(userdata->exhale);
    userdata->encode_func = encode_regular_frame;
    return r;
}

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
    userdata->samples = NULL;
    userdata->packetno = 0;
    userdata->vbr = 3;
    userdata->frame_len = 1024;
    userdata->noise_filling = 1;
    userdata->tune_in_period = 0;
    userdata->encode_func = encode_first_frame;
    return userdata;
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    if(userdata->samples != NULL) free(userdata->samples);
    if(userdata->exhale != NULL) exhaleDelete(userdata->exhale);
    packet_free(&userdata->packet);
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
    membuf dsi = STRBUF_ZERO;
    packet_source me = PACKET_SOURCE_ZERO;
    frame_source_params params = FRAME_SOURCE_PARAMS_ZERO;
    memset(usac_config,0,sizeof(usac_config));

    if( (r = check_sample_rate(source->sample_rate)) < 0) {
        fprintf(stderr,"[encoder:exhale] unsupported sample rate %u\n",source->sample_rate);
        return r;
    }

    me.codec       = CODEC_TYPE_USAC;
    me.channels    = source->channels;
    me.sample_rate = source->sample_rate;
    me.frame_len   = userdata->frame_len;
    me.sync_flag   = 0;
    me.handle      = userdata;
    me.set_params  = plugin_handle_packet_source_params;

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

    if( (r = membuf_ready(&userdata->packet.data,sizeof(uint8_t) * (6144/8) * source->channels)) != 0)
        return r;
    memset(userdata->packet.data.x,0,sizeof(uint8_t) * (6144/8) * source->channels);

    userdata->exhale = exhaleCreate(userdata->samples,userdata->packet.data.x,
      source->sample_rate, source->channels, userdata->frame_len,
      userdata->tune_in_period, userdata->vbr, userdata->noise_filling, 0);
    if(userdata->exhale == NULL) return -1;

    if(exhaleInitEncoder(userdata->exhale,usac_config,&usac_config_size) != 0) return -1;

    dsi.x = usac_config;
    dsi.len = usac_config_size;
    dsi.a = 0;

    if( (r = dest->submit_dsi(dest->handle,&dsi)) != 0) return r;

    params.format   = SAMPLEFMT_S32;
    params.duration = userdata->frame_len;

    userdata->packet.sample_rate = source->sample_rate;

    return source->set_params(source->handle, &params);
}

int plugin_flush(void* ud, const packet_receiver* dest) {
    (void)ud;
    return dest->flush(dest->handle);
}

static int plugin_submit_frame(void* ud, const frame* frame, const packet_receiver* dest) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;
    unsigned int i = 0;
    unsigned int len;
    int32_t *samples;


    samples = (int32_t*)frame_get_channel_samples(frame,0);

    for(i=0;i< frame->duration * frame->channels ;i++) {
        userdata->samples[i] = samples[i] / ( 1 << 8 );
    }

    for( ; i < userdata->frame_len * frame->channels ; i++) {
        userdata->samples[i] = 0;
    }

    len = userdata->encode_func(userdata);

    if(len < 3 || len == 65535) return -1;

    userdata->packet.data.len = len;
    userdata->packet.duration = userdata->frame_len;
    userdata->packet.sync = userdata->packetno++ == 0;

    if(userdata->packetno == userdata->tune_in_period) userdata->packetno = 0;

    if( ( r = dest->submit_packet(dest->handle, &userdata->packet)) != 0) {
        fprintf(stderr,"[encoder:exhale] error sending packet to muxer\n");
    }
    userdata->packet.pts += userdata->frame_len;
    if(userdata->packet.pts > INT64_MAX) userdata->packet.pts -= INT64_MAX;

    return r;
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
