#include "encoder_plugin.h"
#include "muxer_caps.h"

#include <fdk-aac/aacenc_lib.h>
#include "packet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define LOG0(s) fprintf(stderr,"[encoder:fdk-aac] "s"\n")
#define LOG1(s,a) fprintf(stderr,"[encoder:fdk-aac] "s"\n",(a))
#define LOG2(s,a,b) fprintf(stderr,"[encoder:fdk-aac] "s"\n",(a),(b))
#define LOG3(s,a,b,c) fprintf(stderr,"[encoder:fdk-aac] "s"\n",(a),(b),(c))
#define LOGS(s,a) LOG2(s,(int)(a).len,(char *)(a).x)

#define LOGERRNO(s) LOG1(s": %s", strerror(errno))
#define LOGSERRNO(s,a) LOG3(s": %s", (int)a.len, (char *)(a).x, strerror(errno))

#define MAX_CHANNELS 8

static STRBUF_CONST(plugin_name,"fdk-aac");

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
    packet_source me;
};

typedef struct plugin_userdata plugin_userdata;

static size_t plugin_size(void) {
    return sizeof(plugin_userdata);
}

static int plugin_create(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    userdata->aacEncoder = NULL;
    userdata->aot = AOT_AAC_LC;
    userdata->bitrate = 128000;
    userdata->vbr = 0;
    userdata->afterburner = 1;
    packet_init(&userdata->packet);
    packet_init(&userdata->packet2);
    frame_init(&userdata->buffer);
    userdata->me = packet_source_zero;

    return 0;
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

static int plugin_open(void *ud, const frame_source* source, const packet_receiver* dest) {
    int r = 0;
    uint32_t muxer_caps;
    plugin_userdata* userdata = (plugin_userdata*)ud;
    AACENC_ERROR e = AACENC_OK;
    AACENC_InfoStruct info;
    int channel_mode = 0;

    packet_source_info ps_info = PACKET_SOURCE_INFO_ZERO;
    packet_source_params ps_params = PACKET_SOURCE_PARAMS_ZERO;

    ps_info.time_base = source->sample_rate;
    ps_info.frame_len = 1024;

    userdata->packet.sample_rate = source->sample_rate;
    userdata->packet2.sample_rate = source->sample_rate;
    userdata->packet.sample_group = 1;
    userdata->packet2.sample_group = 1;

    userdata->buffer.format = SAMPLEFMT_S16;
    userdata->buffer.channels = channel_count(source->channel_layout);
    userdata->buffer.sample_rate = source->sample_rate;
    userdata->buffer.duration = 0;

    switch(source->channel_layout) {
        case LAYOUT_MONO: channel_mode = 1; break;
        case LAYOUT_STEREO: channel_mode = 2; break;
        case LAYOUT_3_0: channel_mode = 3; break;
        case LAYOUT_4_0: channel_mode = 4; break;
        case LAYOUT_5_0: channel_mode = 5; break;
        case LAYOUT_5_1: channel_mode = 6; break;
        case LAYOUT_7_1: channel_mode = 7; break;
        default: {
            LOG2("unsupported channel layout 0x%lx (%u channels)", source->channel_layout,(unsigned int)channel_count(source->channel_layout));
            return -1;
        }
    }

    if( (r = check_sample_rate(source->sample_rate)) != 0) {
        LOG1("unsupported sample rate %u", source->sample_rate);
        return r;
    }

    if(userdata->aot == AOT_PS && source->channel_layout != LAYOUT_STEREO) {
        LOG0("warning: HE-AACv2 specified but source is not stereo, downgrading to HE-AACv1");
        userdata->aot = AOT_SBR;
    }

    if( (r = dest->get_segment_info(dest->handle, &ps_info, &ps_params)) != 0) {
        LOG0("warning: error getting segment info");
        return r;
    }

    if( (r = frame_ready(&userdata->buffer)) != 0) {
        LOGERRNO("error allocating buffer frame");
        return r;
    }

    muxer_caps = dest->get_caps(dest->handle);

    if( (e = aacEncOpen(&userdata->aacEncoder, 0, 0)) !=  AACENC_OK) {
        LOG1("error opening AAC encoder: %u", e);
        return -1;
    }

    if( (muxer_caps & MUXER_CAP_GLOBAL_HEADERS) == 0) {
        ps_params.packets_per_segment = 0;
    }

    if( (e = aacEncoder_SetParam(userdata->aacEncoder, AACENC_HEADER_PERIOD, ps_params.packets_per_segment)) != AACENC_OK) {
        LOG1("error setting AAC header period %u", e);
    }

    if( (e = aacEncoder_SetParam(userdata->aacEncoder, AACENC_AOT, userdata->aot)) != AACENC_OK) {
        LOG1("error setting AAC AOT: %u", e);
        return -1;
    }

    if( (e = aacEncoder_SetParam(userdata->aacEncoder, AACENC_SAMPLERATE, source->sample_rate)) != AACENC_OK) {
        LOG1("error setting AAC sample rate: %u", e);
        return -1;
    }

    if( (e = aacEncoder_SetParam(userdata->aacEncoder, AACENC_CHANNELORDER, 1)) != AACENC_OK) {
        LOG1("error setting AAC channel mode: %u", e);
        return -1;
    }

    if( (e = aacEncoder_SetParam(userdata->aacEncoder, AACENC_CHANNELMODE, channel_mode)) != AACENC_OK) {
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
    memset(userdata->packet.data.x,0,sizeof(uint8_t) * (6144/8) * MAX_CHANNELS);

    if( (r = membuf_ready(&userdata->packet2.data,sizeof(uint8_t) * (6144/8) * MAX_CHANNELS)) != 0) {
        LOGERRNO("error allocating packet2 buffer");
        return r;
    }
    memset(userdata->packet2.data.x,0,sizeof(uint8_t) * (6144/8) * MAX_CHANNELS);

    userdata->me.codec   = CODEC_TYPE_AAC;
    switch(userdata->aot) {
        case AOT_AAC_LC: {
            userdata->me.profile = CODEC_PROFILE_AAC_LC;
            break;
        }
        case AOT_SBR: {
            userdata->me.profile = CODEC_PROFILE_AAC_HE;
            break;
        }
        case AOT_PS: {
            userdata->me.profile = CODEC_PROFILE_AAC_HE2;
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

    userdata->me.channel_layout = source->channel_layout;
    userdata->me.sample_rate = source->sample_rate;
    userdata->me.frame_len = userdata->frame_len;
    userdata->me.padding = info.nDelay;
    userdata->me.roll_distance = -1;
    userdata->me.sync_flag = 1;
    userdata->me.handle = userdata;

    userdata->me.dsi.x = info.confBuf;
    userdata->me.dsi.len = info.confSize;

    userdata->packet.pts -= (uint64_t) info.nDelay;

    return dest->open(dest->handle, &userdata->me);
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

    return 0;
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

static int plugin_reset(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(userdata->aacEncoder != NULL) aacEncClose(&userdata->aacEncoder);
    userdata->aacEncoder = NULL;

    packet_free(&userdata->packet);
    packet_free(&userdata->packet2);
    frame_free(&userdata->buffer);
    packet_source_free(&userdata->me);

    return 0;
}

static void plugin_close(void *ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    plugin_reset(userdata);
}

static int plugin_init(void) {
    return 0;
}

static void plugin_deinit(void) {
    return;
}

const encoder_plugin encoder_plugin_fdk_aac = {
    plugin_name,
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
