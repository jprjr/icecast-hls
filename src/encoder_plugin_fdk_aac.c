#include "encoder_plugin.h"
#include "muxer_caps.h"

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
    packet packet2;
    frame buffer;
    AUDIO_OBJECT_TYPE aot;
    unsigned int vbr;
    unsigned int bitrate;
    unsigned int afterburner;
    size_t frame_len;
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
    packet_init(&userdata->packet);
    packet_init(&userdata->packet2);
    frame_init(&userdata->buffer);
    return userdata;
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* value) {
    unsigned int mult = 1;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(strbuf_equals_cstr(key,"profile")) {
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

    if(strbuf_equals_cstr(key,"vbr")) {
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

    if(strbuf_equals_cstr(key,"bitrate") || strbuf_equals_cstr(key,"b")) {
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

    if(strbuf_equals_cstr(key,"afterburner")) {
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
    uint32_t muxer_caps;
    plugin_userdata* userdata = (plugin_userdata*)ud;
    AACENC_ERROR e = AACENC_OK;
    AACENC_InfoStruct info;
    packet_source me = PACKET_SOURCE_ZERO;
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

    muxer_caps = dest->get_caps(dest->handle);

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

    if( (e = aacEncoder_SetParam(userdata->aacEncoder, AACENC_SIGNALING_MODE, muxer_caps & MUXER_CAP_GLOBAL_HEADERS ? 2 : 0)) != AACENC_OK) {
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

    if( (r = membuf_ready(&userdata->packet2.data,sizeof(uint8_t) * (6144/8) * MAX_CHANNELS)) != 0) {
        LOGERRNO("error allocating packet2 buffer");
        return r;
    }
    memset(userdata->packet2.data.x,0,sizeof(uint8_t) * (6144/8) * source->channels);

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

    /* TODO: should I use info.nDelay or info.nDelayCore here ?
     *
     * info.nDelay is delay with SBR delay, info.nDelayCore
     * is just the "core" delay.
     *
     * the fdkaac CLI states that including the SBR encoder delay
     * is not iTunes-compatible. The fdkaac CLI also halves
     * the samplerate reported in the MP4 headers and reports
     * nDelay and nDelayCore halved to compensate. Need to test
     * that apple decodes everything properly.
     *
     * the ffmpeg CLI records the original samplerate in the
     * MP4 headers and uses info.nDelay's values in the edit
     * list box.
     */

    me.channels = source->channels;
    me.sample_rate = source->sample_rate;
    me.frame_len = userdata->frame_len;
    me.padding = info.nDelay;
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

    userdata->packet.sample_rate = source->sample_rate;

    userdata->buffer.format = SAMPLEFMT_S16;
    userdata->buffer.channels = source->channels;
    userdata->buffer.sample_rate = source->sample_rate;
    userdata->buffer.duration = 0;
#if 0
    if( (r = frame_fill(&userdata->buffer,me.padding)) != 0) {
        LOGERRNO("error allocating buffer frame");
        return r;
    }
#else
    if( (r = frame_ready(&userdata->buffer)) != 0) {
        LOGERRNO("error allocating buffer frame");
        return r;
    }
#endif

    return 0;
}

static int plugin_encode_frame(plugin_userdata* userdata, const packet_receiver* dest) {

    AACENC_ERROR e = AACENC_OK;
    int16_t* sample_ptr = NULL;
    int r = 0;

    AACENC_BufDesc inBufDesc = { 0 };
    AACENC_BufDesc outBufDesc = { 0 };
    AACENC_InArgs inArgs = { 0 };
    AACENC_OutArgs outArgs = { 0 };

    INT inBufId = IN_AUDIO_DATA;
    INT outBufId = OUT_BITSTREAM_DATA;

    INT inBufSize = sizeof(int16_t) * userdata->buffer.channels * userdata->frame_len;
    INT inBufElSize = sizeof(int16_t);

    INT outBufSize = sizeof(uint8_t) * MAX_CHANNELS * (6144/8);
    INT outBufElSize = sizeof(uint8_t);

    sample_ptr = (int16_t*)frame_get_channel_samples(&userdata->buffer,0);

    inBufDesc.numBufs = 1;
    inBufDesc.bufs = (void **)&sample_ptr;
    inBufDesc.bufferIdentifiers = &inBufId;
    inBufDesc.bufSizes = &inBufSize;
    inBufDesc.bufElSizes = &inBufElSize;

    outBufDesc.numBufs = 1;
    outBufDesc.bufs = (void **)&userdata->packet.data.x;
    outBufDesc.bufferIdentifiers = &outBufId;
    outBufDesc.bufSizes = &outBufSize;
    outBufDesc.bufElSizes = &outBufElSize;

    inArgs.numInSamples = userdata->frame_len * userdata->buffer.channels;
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
        return r;
    }
    userdata->packet.pts += userdata->frame_len;

    return 0;
}

static int plugin_drain(plugin_userdata*  userdata, const packet_receiver* dest) {
    int r;

    while(userdata->buffer.duration >= userdata->frame_len) {
        if( (r = plugin_encode_frame(userdata,dest)) != 0) return r;
        frame_trim(&userdata->buffer,userdata->frame_len);
    }
    return 0;
}

static int plugin_flush(void* ud, const packet_receiver* dest) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;
    unsigned int final_len = userdata->frame_len;
    AACENC_ERROR e = AACENC_OK;

    AACENC_BufDesc inBufDesc = { 0 };
    AACENC_BufDesc outBufDesc = { 0 };
    AACENC_InArgs inArgs = { 0 };
    AACENC_OutArgs outArgs = { 0 };

    INT inBufId = IN_AUDIO_DATA;
    INT outBufId = OUT_BITSTREAM_DATA;

    INT outBufSize = sizeof(uint8_t) * MAX_CHANNELS * (6144/8);
    INT outBufElSize = sizeof(uint8_t);

    if(userdata->buffer.duration > 0) {
        final_len = userdata->buffer.duration;

        if(userdata->buffer.duration < userdata->frame_len) {
            if( (r = frame_fill(&userdata->buffer, userdata->frame_len)) != 0) {
                LOG0("error filling frame buffer");
                return r;
            }
        }
        if( (r = plugin_drain(userdata,dest)) != 0) return r;
    }

    assert(userdata->buffer.duration == 0);

    inBufDesc.numBufs = 0;
    inBufDesc.bufs = NULL;
    inBufDesc.bufferIdentifiers = &inBufId;
    inBufDesc.bufSizes = 0;
    inBufDesc.bufElSizes = 0;

    outBufDesc.numBufs = 1;
    outBufDesc.bufs = (void **)&userdata->packet2.data.x;
    outBufDesc.bufferIdentifiers = &outBufId;
    outBufDesc.bufSizes = &outBufSize;
    outBufDesc.bufElSizes = &outBufElSize;

    inArgs.numInSamples = -1;
    inArgs.numAncBytes = 0;

    if( (e = aacEncEncode(userdata->aacEncoder, &inBufDesc, &outBufDesc, &inArgs, &outArgs)) != AACENC_OK) {
        LOG1("error starting flush: %d",e);
        return -1;
    }

    do {
        userdata->packet2.data.len = outArgs.numOutBytes;
        userdata->packet2.duration = userdata->frame_len;
        userdata->packet2.sync = 1;

        packet_copy(&userdata->packet,&userdata->packet2);

        if( (e = aacEncEncode(userdata->aacEncoder, &inBufDesc, &outBufDesc, &inArgs, &outArgs)) == AACENC_ENCODE_EOF) {
            userdata->packet.duration = final_len;
        }

        if( (r = dest->submit_packet(dest->handle, &userdata->packet)) != 0) {
            LOG0("error sending packet to muxer");
            return r;
        }
        userdata->packet.pts += userdata->packet.duration;

    } while(e == AACENC_OK);

    if(e != AACENC_ENCODE_EOF) {
        LOG1("error flushing final packet: %d",e);
        return -1;
    }

    return dest->flush(dest->handle);
}

static int plugin_submit_frame(void* ud, const frame* frame, const packet_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;

    if( (r = frame_append(&userdata->buffer,frame)) != 0) {
        LOG1("error appending frame to internval buffer: %d",r);
        return r;
    }

    return plugin_drain(userdata,dest);
}

static void plugin_close(void *ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    if(userdata->aacEncoder != NULL) aacEncClose(&userdata->aacEncoder);
    packet_free(&userdata->packet);
    packet_free(&userdata->packet2);
    frame_free(&userdata->buffer);
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
