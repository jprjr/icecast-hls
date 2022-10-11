#include "encoder_plugin.h"

#include <fdk-aac/aacenc_lib.h>
#include "packet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MAX_CHANNELS 2

#define LOG0(s) fprintf(stderr,"[encoder:fdk-aac] "s"\n")
#define LOG1(s,a) fprintf(stderr,"[encoder:fdk-aac] "s"\n",(a))
#define LOG2(s,a,b) fprintf(stderr,"[encoder:fdk-aac] "s"\n",(a),(b))
#define LOG3(s,a,b,c) fprintf(stderr,"[encoder:fdk-aac] "s"\n",(a),(b),(c))
#define LOGS(s,a) LOG2(s,(int)(a).len,(char *)(a).x)

#define LOGERRNO(s) LOG1(s": %s", strerror(errno))
#define LOGSERRNO(s,a) LOG3(s": %s", (int)a.len, (char *)(a).x, strerror(errno))

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
        case  8000: {
            return 0;
        }
        default: break;
    }
    return -1;
}

struct plugin_userdata {
    HANDLE_AACENCODER aacEncoder;
    packet packet;
    AUDIO_OBJECT_TYPE aot;
    unsigned int vbr;
    unsigned int bitrate;
    unsigned int afterburner;
    size_t frame_len;
    int16_t* samples;
};

typedef struct plugin_userdata plugin_userdata;

static void* plugin_create(void) {
    plugin_userdata* userdata = (plugin_userdata*)malloc(sizeof(plugin_userdata));
    if(userdata == NULL) {
        LOGERRNO("unable to allocate plugin userdata");
        return userdata;
    }

    userdata->aacEncoder = NULL;
    userdata->aot = AOT_AAC_LC;
    userdata->bitrate = 128000;
    userdata->vbr = 0;
    userdata->afterburner = 1;
    userdata->samples = 0;
    packet_init(&userdata->packet);
    return userdata;
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* value) {
    unsigned int mult = 1;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(strbuf_ends_cstr(key,"profile")) {
        if(strbuf_caseequals_cstr(value,"aac_low")
            || strbuf_caseequals_cstr(value,"aac")
            || strbuf_caseequals_cstr(value,"aac-lc")
            || strbuf_caseequals_cstr(value,"lc-aac")
            || strbuf_caseequals_cstr(value,"lc")) {
            userdata->aot = AOT_AAC_LC;
            return 0;
        }
        if(strbuf_caseequals_cstr(value,"aac_he")
            || strbuf_caseequals_cstr(value,"he")
            || strbuf_caseequals_cstr(value,"heaac")
            || strbuf_caseequals_cstr(value,"he-aac")
            || strbuf_caseequals_cstr(value,"aac+")
            || strbuf_caseequals_cstr(value,"aacplus")
            || strbuf_caseequals_cstr(value,"aacplus v1")
            || strbuf_caseequals_cstr(value,"aacplus-v1")
            || strbuf_caseequals_cstr(value,"aacplusv1")) {
            userdata->aot = AOT_SBR;
            return 0;
        }
        if(strbuf_caseequals_cstr(value,"aac_he_v2")
            || strbuf_caseequals_cstr(value,"hev2")
            || strbuf_caseequals_cstr(value,"heaac2")
            || strbuf_caseequals_cstr(value,"he-aac2")
            || strbuf_caseequals_cstr(value,"he-aacv2")
            || strbuf_caseequals_cstr(value,"eaac+")
            || strbuf_caseequals_cstr(value,"aacplus v2")
            || strbuf_caseequals_cstr(value,"aacplus-v2")
            || strbuf_caseequals_cstr(value,"aacplusv2")) {
            userdata->aot = AOT_PS;
            return 0;
        }
        LOGS("unknown/unsupported value for profile: %.*s",*value);
        return -1;
    }

    if(strbuf_ends_cstr(key,"vbr")) {
        errno = 0;
        userdata->vbr = strbuf_strtoul(value,10);
        if(errno != 0) {
            LOGSERRNO("error parsing vbr value %.*s",(*value));
            return -1;
        }
        if(userdata->vbr > 5) {
            LOGS("invalid value for vbr: %.*s (max 5)",(*value));
            return -1;
        }
        return 0;
    }

    if(strbuf_ends_cstr(key,"bitrate") || strbuf_ends_cstr(key,"-b") || strbuf_equals_cstr(key,"b")) {
        if(strbuf_caseends_cstr(value,"k")) {
            mult = 1000;
        }
        if(strbuf_caseends_cstr(value,"kbps")) {
            mult = 1000;
        }
        errno = 0;
        userdata->bitrate = strbuf_strtoul(value,10);
        if(errno != 0) {
            LOGSERRNO("error parsing bitrate value %.*s",(*value));
            return -1;
        }
        userdata->bitrate *= mult;
        return 0;
    }

    if(strbuf_ends_cstr(key,"afterburner")) {
        if(strbuf_truthy(value)) {
            userdata->afterburner = 1;
            return 0;
        }
        if(strbuf_falsey(value)) {
            userdata->afterburner = 0;
            return 0;
        }
        LOGS("unknown/unsupported value for afterburner: %.*s",*value);
        return -1;
    }

    LOGS("unknown key: %.*s\n",*key);
    return -1;
}

static int plugin_handle_packet_source_params(void* ud, const packet_source_params* params) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    AACENC_ERROR e = AACENC_OK;
    if( (e = aacEncoder_SetParam(userdata->aacEncoder, AACENC_HEADER_PERIOD, params->packets_per_segment)) != AACENC_OK) {
        LOG1("error setting AAC header period %u", e);
    }

    if( (e = aacEncEncode(userdata->aacEncoder, NULL, NULL, NULL, NULL)) != AACENC_OK) {
        LOG1("error re-initializing AAC encoder: %u", e);
        return -1;
    }

    return 0;
}

static int plugin_open(void *ud, const frame_source* source, const packet_receiver* dest) {
    int r = 0;
    plugin_userdata* userdata = (plugin_userdata*)ud;
    AACENC_ERROR e = AACENC_OK;
    AACENC_InfoStruct info;
    packet_source me = PACKET_SOURCE_ZERO;
    packet_receiver_caps receiver_caps = PACKET_RECEIVER_CAPS_ZERO;
    frame_source_params params = FRAME_SOURCE_PARAMS_ZERO;
    membuf dsi = MEMBUF_ZERO;

    if(source->channels > MAX_CHANNELS) {
        LOG2("unsupported channel config - requested %u channels, max is %u channels",(unsigned int)source->channels, (unsigned int)MAX_CHANNELS);
        return -1;
    }

    if( (r = check_sample_rate(source->sample_rate)) != 0) {
        LOG1("unsupported sample rate %u", source->sample_rate);
        return r;
    }

    if(userdata->aot == AOT_PS && source->channels != 2) {
        LOG0("warning: HE-AACv2 specified but source is mono, downgrading to HE-AACv1");
        userdata->aot = AOT_SBR;
    }

    if( (r = dest->get_caps(dest->handle,&receiver_caps)) != 0) {
        return r;
    }

    if( (e = aacEncOpen(&userdata->aacEncoder, 0, 0)) !=  AACENC_OK) {
        LOG1("error opening AAC encoder: %u", e);
        return -1;
    }

    if( (e = aacEncoder_SetParam(userdata->aacEncoder, AACENC_AOT, userdata->aot)) != AACENC_OK) {
        LOG1("error setting AAC AOT: %u", e);
        return -1;
    }

    if( (e = aacEncoder_SetParam(userdata->aacEncoder, AACENC_SAMPLERATE, source->sample_rate)) != AACENC_OK) {
        LOG1("error setting AAC sample rate: %u", e);
        return -1;
    }

    if( (e = aacEncoder_SetParam(userdata->aacEncoder, AACENC_CHANNELMODE, source->channels)) != AACENC_OK) {
        LOG1("error setting AAC channel mode: %u", e);
        return -1;
    }

    if( (e = aacEncoder_SetParam(userdata->aacEncoder, AACENC_BITRATEMODE, userdata->vbr)) != AACENC_OK) {
        LOG1("error setting AAC vbr mode: %u", e);
        return -1;
    }

    if( (e = aacEncoder_SetParam(userdata->aacEncoder, AACENC_BITRATE, userdata->bitrate)) != AACENC_OK) {
        LOG1("error setting AAC bitrate: %u", e);
        return -1;
    }

    if( (e = aacEncoder_SetParam(userdata->aacEncoder, AACENC_AFTERBURNER, userdata->afterburner)) != AACENC_OK) {
        LOG1("error setting AAC bitrate: %u", e);
        return -1;
    }

    if( (e = aacEncoder_SetParam(userdata->aacEncoder, AACENC_TRANSMUX, 0)) != AACENC_OK) {
        LOG1("error setting AAC transmux: %u", e);
        return -1;
    }

    if( (e = aacEncoder_SetParam(userdata->aacEncoder, AACENC_SIGNALING_MODE, receiver_caps.has_global_header ? 2 : 0)) != AACENC_OK) {
        LOG1("error setting AAC signaling mode: %u", e);
        return -1;
    }

    if( (e = aacEncEncode(userdata->aacEncoder, NULL, NULL, NULL, NULL)) != AACENC_OK) {
        LOG1("error initializing AAC encoder: %u", e);
        return -1;
    }

    if( (e = aacEncInfo(userdata->aacEncoder, &info)) != AACENC_OK) {
        LOG1("error initializing AAC encoder: %u", e);
        return -1;
    }

    userdata->frame_len = info.frameLength;

    if( (r = membuf_ready(&userdata->packet.data,sizeof(uint8_t) * (6144/8) * MAX_CHANNELS)) != 0) {
        LOGERRNO("error allocating packet buffer");
        return r;
    }
    memset(userdata->packet.data.x,0,sizeof(uint8_t) * (6144/8) * source->channels);

    userdata->samples = (int16_t*)malloc(sizeof(int16_t) * MAX_CHANNELS * userdata->frame_len);
    if(userdata->samples == NULL) {
        LOGERRNO("error allocating sample buffer");
        return r;
    }

    me.codec   = CODEC_TYPE_AAC;
    switch(userdata->aot) {
        case AOT_AAC_LC: {
            me.profile = CODEC_PROFILE_AAC_LC;
            break;
        }
        case AOT_SBR: {
            me.profile = CODEC_PROFILE_AAC_HE;
            break;
        }
        case AOT_PS: {
            me.profile = CODEC_PROFILE_AAC_HE2;
            break;
        }
        default: {
            LOG0("unsupported profile?!");
            return -1;
        }
    }

    /* TODO: should I use info.nDelay or info.nDelayCore here ? */

    me.channels = source->channels;
    me.sample_rate = source->sample_rate;
    me.frame_len = userdata->frame_len;
    me.padding = info.nDelay;
    if(me.profile == CODEC_PROFILE_AAC_HE2 || me.profile == CODEC_PROFILE_AAC_HE) {
        /* I think delay is "doubled" since SBR halves the sampling rate? */
        me.padding >>= 1;
    }
    me.roll_distance = -1;
    me.sync_flag = 1;
    me.handle = userdata;
    me.set_params = plugin_handle_packet_source_params;

    if(( r = dest->open(dest->handle, &me)) != 0) {
        LOG0("error opening muxer");
        return r;
    }

    dsi.x = info.confBuf;
    dsi.len = info.confSize;

    if(( r = dest->submit_dsi(dest->handle,&dsi)) != 0) {
        return r;
    }

    params.format = SAMPLEFMT_S16;
    params.duration = userdata->frame_len;

    userdata->packet.sample_rate = source->sample_rate;

    return source->set_params(source->handle, &params);
}

static int plugin_flush(void* ud, const packet_receiver* dest) {
    (void)ud;
    return dest->flush(dest->handle);
}

static int plugin_submit_frame(void* ud, const frame* frame, const packet_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    AACENC_ERROR e = AACENC_OK;
    int16_t* sample_ptr = NULL;
    size_t i = 0;
    int r = 0;

    AACENC_BufDesc inBufDesc = { 0 };
    AACENC_BufDesc outBufDesc = { 0 };
    AACENC_InArgs inArgs = { 0 };
    AACENC_OutArgs outArgs = { 0 };

    INT inBufId = IN_AUDIO_DATA;
    INT outBufId = OUT_BITSTREAM_DATA;

    INT inBufSize = sizeof(int16_t) * frame->channels * userdata->frame_len;
    INT inBufElSize = sizeof(int16_t);
    
    INT outBufSize = sizeof(uint8_t) * MAX_CHANNELS * (6144/8);
    INT outBufElSize = sizeof(uint8_t);

    sample_ptr = (int16_t*)frame_get_channel_samples(frame,0);

    for(i=0; i< frame->duration * frame->channels; i++) {
        userdata->samples[i] = sample_ptr[i];
    }
    for(;i<userdata->frame_len * frame->channels;i++) {
        userdata->samples[i] = 0;
    }

    inBufDesc.numBufs = 1;
    inBufDesc.bufs = (void **)&userdata->samples;
    inBufDesc.bufferIdentifiers = &inBufId;
    inBufDesc.bufSizes = &inBufSize;
    inBufDesc.bufElSizes = &inBufElSize;

    outBufDesc.numBufs = 1;
    outBufDesc.bufs = (void **)&userdata->packet.data.x;
    outBufDesc.bufferIdentifiers = &outBufId;
    outBufDesc.bufSizes = &outBufSize;
    outBufDesc.bufElSizes = &outBufElSize;

    inArgs.numInSamples = userdata->frame_len * frame->channels;
    inArgs.numAncBytes = 0;

    if( (e = aacEncEncode(userdata->aacEncoder, &inBufDesc, &outBufDesc, &inArgs, &outArgs)) != AACENC_OK) {
        LOG1("error encoding audio frame: %u", e);
        return -1;
    }

    userdata->packet.data.len = outArgs.numOutBytes;
    userdata->packet.duration = userdata->frame_len;
    userdata->packet.sync = 1;

    if( (r = dest->submit_packet(dest->handle, &userdata->packet)) != 0) {
        LOG0("error sending packet to muxer");
    }
    userdata->packet.pts += userdata->frame_len;
    return r;

}

static void plugin_close(void *ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    if(userdata->aacEncoder != NULL) aacEncClose(&userdata->aacEncoder);
    packet_free(&userdata->packet);
}

static int plugin_init(void) {
    return 0;
}

static void plugin_deinit(void) {
    return;
}

const encoder_plugin encoder_plugin_fdk_aac = {
    { .a = 0, .len = 7, .x = (uint8_t*)"fdk-aac" },
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_submit_frame,
    plugin_flush,
};
