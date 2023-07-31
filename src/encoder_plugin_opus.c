#include "encoder_plugin_opus.h"
#include "muxer_caps.h"

#include <opus.h>
#include <opus_multistream.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>

#include "pack_u16le.h"
#include "pack_u32le.h"
#include "version.h"
#include "vorbis_mappings.h"

#define LOG_PREFIX "[encoder:exhale]"
#include "logger.h"

/* TODO switch to the multistream API and go up to 8 channels */
#define MAX_CHANNELS 8
#define SAMPLE_RATE 48000

/* per RFC7845 (Ogg Encapsulation for the Opus Audio Codec):
 * Demuxers SHOULD treat audio data packets as invalid (treat them as if
 * they were malformed Opus packets with an invalid TOC sequence) if
 * they are larger than 61,440 octets per Opus stream, unless they have
 * a specific reason for allowing extra padding.
 *
 * So technically MAX_PACKET could be smaller since Opus will probably combine
 * some channels into single streams - for example, for 3.0 audio there will be
 * two streams - a stereo stream for the left and right channels, and a mono
 * stream for the center. I figure just allocating for 8 mono streams is safe. */
#define MAX_PACKET (61440 * MAX_CHANNELS)

#define LOGS(s, a) log_error(s, (int)(a).len, (char *)(a).x);
#define LOGERRNO(s) log_error(s": %s", strerror(errno))
#define LOGSERRNO(s,a) log_error(s": %s", (int)a.len, (char *)(a).x, strerror(errno))

static STRBUF_CONST(plugin_name,"opus");

struct encoder_plugin_opus_userdata {
    OpusMSEncoder* enc;
    packet packet;

    frame buffer;
    frame samples;
    int complexity;
    int signal;
    int vbr;
    int vbr_constraint;
    int application;
    opus_int32 bitrate;

    unsigned int framelen;
    unsigned int channels;
    strbuf name;

    packet_source me;
    int streams;
    int coupled_streams;
    uint8_t mapping[255]; /* we shouldn't actually use over 8 but I'm unsure
    if the opus encoder expects more? */
};

typedef struct encoder_plugin_opus_userdata encoder_plugin_opus_userdata;

static size_t encoder_plugin_opus_size(void) {
    return sizeof(encoder_plugin_opus_userdata);
}

static int encoder_plugin_opus_create(void* ud) {
    encoder_plugin_opus_userdata* userdata = (encoder_plugin_opus_userdata*)ud;

    userdata->enc = NULL;
    packet_init(&userdata->packet);
    frame_init(&userdata->buffer);
    frame_init(&userdata->samples);
    strbuf_init(&userdata->name);

    userdata->complexity = 10;
    userdata->signal = OPUS_SIGNAL_MUSIC;
    userdata->vbr = 1;
    userdata->vbr_constraint = 0;
    userdata->application = OPUS_APPLICATION_AUDIO;
    userdata->bitrate = 96000;
    userdata->framelen = 960;
    userdata->channels = 0;
    userdata->streams = 0;
    userdata->coupled_streams = 0;
    userdata->me = packet_source_zero;

    return 0;
}

static int encoder_plugin_opus_reset(void* ud) {
    encoder_plugin_opus_userdata* userdata = (encoder_plugin_opus_userdata*)ud;

    packet_free(&userdata->packet);
    frame_free(&userdata->buffer);
    frame_free(&userdata->samples);
    strbuf_free(&userdata->name);

    if(userdata->enc != NULL) {
        opus_multistream_encoder_destroy(userdata->enc);
        userdata->enc = NULL;
    }
    packet_source_free(&userdata->me);
    return 0;
}

static void encoder_plugin_opus_close(void* ud) {
    encoder_plugin_opus_userdata* userdata = (encoder_plugin_opus_userdata*)ud;
    encoder_plugin_opus_reset(userdata);
}

static int encoder_plugin_opus_config(void* ud, const strbuf* key, const strbuf* value) {
    encoder_plugin_opus_userdata* userdata = (encoder_plugin_opus_userdata*)ud;

    unsigned int mult = 1;

    if(strbuf_equals_cstr(key,"complexity")) {
        errno = 0;
        userdata->complexity = strbuf_strtoul(value,10);
        if(errno != 0) {
            LOGSERRNO("error parsing complexity value %.*s",(*value));
            return -1;
        }
        if(userdata->complexity > 10) {
            LOGS("invalid value for complexity: %.*s (max 10)",(*value));
            return -1;
        }
        return 0;
    }

    if(strbuf_equals_cstr(key,"signal")) {
        if(strbuf_caseequals_cstr(value,"auto")) {
            userdata->signal = OPUS_AUTO;
        } else if(strbuf_caseequals_cstr(value,"voice")) {
            userdata->signal = OPUS_SIGNAL_VOICE;
        } else if(strbuf_caseequals_cstr(value,"music")) {
            userdata->signal = OPUS_SIGNAL_MUSIC;
        } else {
            LOGS("invalid value for signal: %.*s [auto | voice | music]",(*value));
            return -1;
        }
        return 0;
    }

    if(strbuf_equals_cstr(key,"application")) {
        if(strbuf_caseequals_cstr(value,"voip")) {
            userdata->signal = OPUS_APPLICATION_VOIP;
        } else if(strbuf_caseequals_cstr(value,"audio")) {
            userdata->application = OPUS_APPLICATION_AUDIO;
        } else if(strbuf_caseequals_cstr(value,"lowdelay")) {
            userdata->application = OPUS_APPLICATION_RESTRICTED_LOWDELAY;
        } else {
            LOGS("invalid value for application: %.*s [voip | audio | lowdelay]",(*value));
            return -1;
        }
        return 0;
    }

    if(strbuf_equals_cstr(key,"vbr")) {
        if(strbuf_truthy(value)) {
            userdata->vbr = 1;
            return 0;
        }
        if(strbuf_falsey(value)) {
            userdata->vbr = 0;
            return 0;
        }
        LOGS("invalid value for vbr: %.*s [boolean]",(*value));
        return -1;
    }

    if(strbuf_equals_cstr(key,"vbr-constraint") ||
       strbuf_equals_cstr(key,"vbr constraint") ||
       strbuf_equals_cstr(key,"vbr-constrained") ||
       strbuf_equals_cstr(key,"vbr constrained") ||
       strbuf_equals_cstr(key,"constrained-vbr") ||
       strbuf_equals_cstr(key,"constrained vbr") ||
       strbuf_equals_cstr(key,"constrain-vbr") ||
       strbuf_equals_cstr(key,"constrain vbr")) {
        if(strbuf_truthy(value)) {
            userdata->vbr_constraint = 1;
            return 0;
        }
        if(strbuf_falsey(value)) {
            userdata->vbr_constraint = 0;
            return 0;
        }
        LOGS("invalid value for vbr-constraint: %.*s",(*value));
        return -1;
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

    LOGS("unknown key: %.*s\n",*key);
    return -1;
}

static int encoder_plugin_opus_configure(encoder_plugin_opus_userdata* userdata) {
    int err;
    userdata->enc = opus_multistream_surround_encoder_create(48000, userdata->channels, userdata->channels > 2 ? 1 : 0, &userdata->streams, &userdata->coupled_streams, userdata->mapping, userdata->application, &err);
    if(userdata->enc == NULL) {
        log_error("error creating opus encoder: %s",opus_strerror(err));
        return -1;
    }

    opus_multistream_encoder_ctl(userdata->enc,OPUS_SET_BITRATE(userdata->bitrate));
    opus_multistream_encoder_ctl(userdata->enc,OPUS_SET_SIGNAL(userdata->signal));
    opus_multistream_encoder_ctl(userdata->enc,OPUS_SET_COMPLEXITY(userdata->complexity));
    opus_multistream_encoder_ctl(userdata->enc,OPUS_SET_VBR(userdata->vbr));
    opus_multistream_encoder_ctl(userdata->enc,OPUS_SET_VBR_CONSTRAINT(userdata->vbr_constraint));

    return 0;
}

static int opus_drain(encoder_plugin_opus_userdata* userdata, const packet_receiver* dest, unsigned int framelen) {
    int r = 0;
    size_t i = 0;
    size_t j = 0;
    size_t c = 0;
    float* src = NULL;
    float* samples = NULL;
    opus_int32 result = 0;

    samples = (float*)frame_get_channel_samples(&userdata->samples, 0);

    while(userdata->buffer.duration >= userdata->framelen) {

        for(i=0;i<userdata->channels;i++) {
            c = (size_t)vorbis_channel_layout[userdata->channels][i];
            src = frame_get_channel_samples(&userdata->buffer, i);
            for(j=0;j<userdata->framelen;j++) {
                samples[(j * ((size_t)userdata->channels)) + c] = src[j];
            }
        }

        result = opus_multistream_encode_float(userdata->enc, samples, userdata->framelen, userdata->packet.data.x,MAX_PACKET);
        if(result <= 0) {
            log_error("received error in opus_encode: %s", opus_strerror(result));
            return -1;
        }

        frame_trim(&userdata->buffer,userdata->framelen);

        userdata->packet.data.len = result;
        userdata->packet.duration = framelen;
        userdata->packet.sync = 1;
        userdata->packet.sample_group = 1;

        if( (r = dest->submit_packet(dest->handle, &userdata->packet)) != 0) {
            logs_error("error sending packet to muxer");
            return r;
        }

        userdata->packet.pts += userdata->framelen;
    }
    return 0;
}

static int encoder_plugin_opus_open(void *ud, const frame_source* source, const packet_receiver* dest) {
    int r = -1;
    uint32_t muxer_caps;
    opus_int32 lookahead;
    encoder_plugin_opus_userdata* userdata = (encoder_plugin_opus_userdata*)ud;

    switch(source->channel_layout) {
        case LAYOUT_MONO:   userdata->channels = 1; break;
        case LAYOUT_STEREO: userdata->channels = 2; break;
        case LAYOUT_3_0:    userdata->channels = 3; break;
        case LAYOUT_4_0:    userdata->channels = 4; break;
        case LAYOUT_5_0:    userdata->channels = 5; break;
        case LAYOUT_5_1:    userdata->channels = 6; break;
        case LAYOUT_6_1:    userdata->channels = 7; break;
        case LAYOUT_7_1:    userdata->channels = 8; break;
        default: {
            log_error("unsupported channel layout 0x%" PRIx64, source->channel_layout);
            return -1;
        }
    }

    if(userdata->channels > MAX_CHANNELS) {
        log_error("unsupported channel config - requested %u channels, max is %u channels",userdata->channels, (unsigned int)MAX_CHANNELS);
        return r;
    }

    if(source->sample_rate != 48000) {
        log_error("unsupported sample rate %u", source->sample_rate);
        return r;
    }

    muxer_caps = dest->get_caps(dest->handle);

    if(!(muxer_caps & MUXER_CAP_GLOBAL_HEADERS)) {
        logs_error("selected muxer does not have global header, select a different muxer");
        return -1;
    }

    if( (r = membuf_ready(&userdata->packet.data,sizeof(uint8_t) * MAX_PACKET)) != 0) {
        LOGERRNO("error allocating packet buffer");
        return r;
    }

    userdata->packet.sample_rate = 48000;

    if( (r = encoder_plugin_opus_configure(userdata)) != 0) {
        return r;
    }
    opus_multistream_encoder_ctl(userdata->enc,OPUS_GET_LOOKAHEAD(&lookahead));

    userdata->samples.format = SAMPLEFMT_FLOAT;
    userdata->samples.channels = userdata->channels;
    userdata->samples.duration = userdata->framelen;
    userdata->samples.sample_rate = 48000;

    if( (r = frame_buffer(&userdata->samples)) != 0) {
        LOGERRNO("error allocating samples frame");
        return r;
    }

    userdata->buffer.format = SAMPLEFMT_FLOATP;
    userdata->buffer.channels = userdata->channels;
    userdata->buffer.duration = 0;
    userdata->buffer.sample_rate = 48000;


    if( (r = frame_ready(&userdata->buffer)) != 0) {
        LOGERRNO("error allocating buffer frame");
        return r;
    }

    if( (r = frame_fill(&userdata->buffer,lookahead)) != 0) {
        LOGERRNO("error allocating buffer frame");
        return r;
    }

    if( (r = strbuf_append_cstr(&userdata->name,opus_get_version_string())) != 0) {
        LOGERRNO("error appending name string");
        return r;
    }
    if( (r = strbuf_append_cstr(&userdata->name,", ")) != 0) {
        LOGERRNO("error appending name string");
        return r;
    }
    if( (r = strbuf_append_cstr(&userdata->name,icecast_hls_version_string())) != 0) {
        LOGERRNO("error appending name string");
        return r;
    }

    if( (r = membuf_ready(&userdata->me.dsi,19 + (userdata->channels > 2 ? 2 + userdata->channels : 0 ))) != 0) {
        LOGERRNO("error allocating dsi");
        return -1;
    }

    memcpy(&userdata->me.dsi.x[0],"OpusHead",8);
    userdata->me.dsi.x[8] = 1;
    userdata->me.dsi.x[9] = userdata->channels;
    pack_u16le(&userdata->me.dsi.x[10],lookahead);
    pack_u32le(&userdata->me.dsi.x[12],48000);
    userdata->me.dsi.x[16] = 0;
    userdata->me.dsi.x[17] = 0;
    userdata->me.dsi.x[18] = userdata->channels > 2 ? 1 : 0;
    userdata->me.dsi.len = 19;

    if(userdata->channels > 2) {
        userdata->me.dsi.x[userdata->me.dsi.len++] = (uint8_t)userdata->streams;
        userdata->me.dsi.x[userdata->me.dsi.len++] = (uint8_t)userdata->coupled_streams;
        if( (r = membuf_append(&userdata->me.dsi, userdata->mapping, userdata->channels)) != 0) {
            LOGERRNO("error appending channel mapping");
            return -1;
        }
    }

    userdata->me.codec = CODEC_TYPE_OPUS;
    userdata->me.name = &userdata->name;
    userdata->me.channel_layout = source->channel_layout;
    userdata->me.sample_rate = 48000;
    userdata->me.frame_len = userdata->framelen;
    userdata->me.padding = lookahead;
    userdata->me.roll_distance = -3840 / 960;
    userdata->me.sync_flag = 1;
    userdata->me.handle = userdata;

    userdata->packet.pts -= (uint64_t)lookahead;

    return dest->open(dest->handle,&userdata->me);
}


static int encoder_plugin_opus_flush(void* ud, const packet_receiver* dest) {
    int r = 0;
    unsigned int framelen;
    encoder_plugin_opus_userdata* userdata = (encoder_plugin_opus_userdata*)ud;

    if(userdata->buffer.duration > 0) {
        if( (r = opus_drain(userdata,dest,userdata->framelen)) != 0) return r;

        if(userdata->buffer.duration < userdata->framelen) {
            framelen = userdata->framelen;
            if( (r = frame_fill(&userdata->buffer,userdata->framelen)) != 0) {
                logs_fatal("error filling frame buffer");
                return r;
            }
            if( (r = opus_drain(userdata,dest,framelen)) != 0) return r;
        }
    }

    return 0;
}

static int encoder_plugin_opus_submit_frame(void* ud, const frame* frame, const packet_receiver* dest) {
    int r;
    encoder_plugin_opus_userdata* userdata = (encoder_plugin_opus_userdata*)ud;

    if( (r = frame_append(&userdata->buffer,frame)) != 0) {
        log_error("error appending frame to internal buffer: %d",r);
        return r;
    }

    return opus_drain(userdata,dest,userdata->framelen);
}

static int encoder_plugin_opus_init(void) {
    return 0;
}

static void encoder_plugin_opus_deinit(void) {
    return;
}

const encoder_plugin encoder_plugin_opus = {
    plugin_name,
    encoder_plugin_opus_size,
    encoder_plugin_opus_init,
    encoder_plugin_opus_deinit,
    encoder_plugin_opus_create,
    encoder_plugin_opus_config,
    encoder_plugin_opus_open,
    encoder_plugin_opus_close,
    encoder_plugin_opus_submit_frame,
    encoder_plugin_opus_flush,
    encoder_plugin_opus_reset,
};
