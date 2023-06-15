#include "encoder_plugin_opus.h"
#include "muxer_caps.h"

#include <opus.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "pack_u16le.h"
#include "pack_u32le.h"
#include "version.h"

#define MAX_CHANNELS 2
#define SAMPLE_RATE 48000
#define MAX_PACKET 61440

#define LOG0(s) fprintf(stderr,"[encoder:opus] "s"\n")
#define LOG1(s,a) fprintf(stderr,"[encoder:opus] "s"\n",(a))
#define LOG2(s,a,b) fprintf(stderr,"[encoder:opus] "s"\n",(a),(b))
#define LOG3(s,a,b,c) fprintf(stderr,"[encoder:opus] "s"\n",(a),(b),(c))
#define LOGS(s,a) LOG2(s,(int)(a).len,(char *)(a).x)

#define LOGERRNO(s) LOG1(s": %s", strerror(errno))
#define LOGSERRNO(s,a) LOG3(s": %s", (int)a.len, (char *)(a).x, strerror(errno))

struct encoder_plugin_opus_userdata {
    OpusEncoder* enc;
    packet packet;
    membuf dsi;

    frame buffer;
    int complexity;
    int signal;
    int vbr;
    int vbr_constraint;
    int application;
    opus_int32 bitrate;

    unsigned int framelen;
    unsigned int keyframes;
    unsigned int channels;
    strbuf name;
};

typedef struct encoder_plugin_opus_userdata encoder_plugin_opus_userdata;

static void* encoder_plugin_opus_create(void) {
    encoder_plugin_opus_userdata* userdata = (encoder_plugin_opus_userdata*)malloc(sizeof(encoder_plugin_opus_userdata));
    if(userdata == NULL) {
        LOGERRNO("unable to allocate userdata");
        return NULL;
    }

    userdata->enc = NULL;
    packet_init(&userdata->packet);
    membuf_init(&userdata->dsi);
    frame_init(&userdata->buffer);
    strbuf_init(&userdata->name);

    userdata->complexity = 10;
    userdata->signal = OPUS_SIGNAL_MUSIC;
    userdata->vbr = 1;
    userdata->vbr_constraint = 0;
    userdata->application = OPUS_APPLICATION_AUDIO;
    userdata->bitrate = 96000;
    userdata->keyframes = 0;
    userdata->framelen = 960;
    userdata->channels = 0;

    return userdata;
}

static void encoder_plugin_opus_close(void* ud) {
    encoder_plugin_opus_userdata* userdata = (encoder_plugin_opus_userdata*)ud;

    packet_free(&userdata->packet);
    membuf_free(&userdata->dsi);
    frame_free(&userdata->buffer);
    strbuf_free(&userdata->name);

    if(userdata->enc != NULL) {
        opus_encoder_destroy(userdata->enc);
    }
    free(userdata);
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

static int encoder_plugin_opus_set_keyframes(void* ud, unsigned int keyframes) {
    encoder_plugin_opus_userdata* userdata = (encoder_plugin_opus_userdata*)ud;
    userdata->keyframes = keyframes;
    return 0;
}

static int encoder_plugin_opus_configure(encoder_plugin_opus_userdata* userdata) {
    int err;
    userdata->enc = opus_encoder_create(48000, userdata->channels, userdata->application, &err);
    if(userdata->enc == NULL) {
        LOG1("error creating opus encoder: %s",opus_strerror(err));
        return -1;
    }

    opus_encoder_ctl(userdata->enc,OPUS_SET_BITRATE(userdata->bitrate));
    opus_encoder_ctl(userdata->enc,OPUS_SET_SIGNAL(userdata->signal));
    opus_encoder_ctl(userdata->enc,OPUS_SET_COMPLEXITY(userdata->complexity));
    opus_encoder_ctl(userdata->enc,OPUS_SET_VBR(userdata->vbr));
    opus_encoder_ctl(userdata->enc,OPUS_SET_VBR_CONSTRAINT(userdata->vbr_constraint));

    return 0;
}

static int opus_drain(encoder_plugin_opus_userdata* userdata, const packet_receiver* dest) {
    int r = 0;
    opus_int32 result = 0;
    opus_int32 pred;

    while(userdata->buffer.duration >= userdata->framelen) {
        if(userdata->keyframes) {
            opus_encoder_ctl(userdata->enc, OPUS_GET_PREDICTION_DISABLED(&pred));
            opus_encoder_ctl(userdata->enc, OPUS_SET_PREDICTION_DISABLED(1));
        }

        result = opus_encode_float(userdata->enc, frame_get_channel_samples(&userdata->buffer,0), userdata->framelen, userdata->packet.data.x,MAX_PACKET);
        if(result <= 0) {
            LOG1("received error in opus_encode: %s", opus_strerror(result));
            return -1;
        }
        if(userdata->keyframes) {
            opus_encoder_ctl(userdata->enc, OPUS_SET_PREDICTION_DISABLED(pred));
            userdata->keyframes--;
        }

        frame_trim(&userdata->buffer,userdata->framelen);

        userdata->packet.data.len = result;
        userdata->packet.duration = userdata->framelen;
        userdata->packet.sync = 1;

        if( (r = dest->submit_packet(dest->handle, &userdata->packet)) != 0) {
            LOG0("error sending packet to muxer");
            return r;
        }

        userdata->packet.pts += userdata->framelen;
    }
    return 0;
}

struct reset_wrapper {
    void* dest;
    int (*cb)(void*, const packet*);
};

typedef struct reset_wrapper reset_wrapper;

static int reset_wrapper_cb(void* handle, const packet* p) {
    reset_wrapper* wrap = (reset_wrapper*) handle;
    return wrap->cb(wrap->dest,p);
}

static int encoder_plugin_opus_reset(void* ud, void* dest, int (*cb)(void*, const packet*)) {
    int r;
    encoder_plugin_opus_userdata* userdata = (encoder_plugin_opus_userdata*)ud;

    reset_wrapper wrap;
    wrap.dest = dest;
    wrap.cb = cb;

    packet_receiver recv;
    recv.handle = &wrap;
    recv.submit_packet = reset_wrapper_cb;

    if(userdata->enc != NULL) {
        if(userdata->buffer.duration > 0) {
            if(userdata->buffer.duration < userdata->framelen) {
                if( (r = frame_fill(&userdata->buffer,userdata->framelen)) != 0) {
                    LOG0("error filling frame buffer");
                    return r;
                }
            }
            if( (r = opus_drain(userdata,&recv)) != 0) return r;
        }
        opus_encoder_destroy(userdata->enc);
        userdata->enc = NULL;
    }

    return encoder_plugin_opus_configure(userdata);
}

static int encoder_plugin_opus_open(void *ud, const frame_source* source, const packet_receiver* dest) {
    int r = -1;
    uint32_t muxer_caps;
    opus_int32 lookahead;
    encoder_plugin_opus_userdata* userdata = (encoder_plugin_opus_userdata*)ud;

    packet_source me = PACKET_SOURCE_ZERO;

    if(source->channels < 1 || source->channels > MAX_CHANNELS) {
        LOG2("unsupported channel config - requested %u channels, max is %u channels",(unsigned int)source->channels, (unsigned int)MAX_CHANNELS);
        return r;
    }

    if(source->sample_rate != 48000) {
        LOG1("unsupported sample rate %u", source->sample_rate);
        return r;
    }

    muxer_caps = dest->get_caps(dest->handle);

    if(!(muxer_caps & MUXER_CAP_GLOBAL_HEADERS)) {
        LOG0("selected muxer does not have global header, select a different muxer");
        return -1;
    }

    userdata->channels = source->channels;

    if( (r = membuf_ready(&userdata->packet.data,sizeof(uint8_t) * MAX_PACKET)) != 0) {
        LOGERRNO("error allocating packet buffer");
        return r;
    }

    userdata->packet.sample_rate = 48000;

    if( (r = encoder_plugin_opus_configure(userdata)) != 0) {
        return r;
    }
    opus_encoder_ctl(userdata->enc,OPUS_GET_LOOKAHEAD(&lookahead));

    userdata->buffer.format = SAMPLEFMT_FLOAT;
    userdata->buffer.channels = source->channels;
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

    if( (r = membuf_ready(&userdata->dsi,19)) != 0) {
        LOGERRNO("error allocating dsi");
        return -1;
    }

    memcpy(&userdata->dsi.x[0],"OpusHead",8);
    userdata->dsi.x[8] = 1;
    userdata->dsi.x[9] = source->channels;
    pack_u16le(&userdata->dsi.x[10],lookahead);
    pack_u32le(&userdata->dsi.x[12],48000);
    userdata->dsi.x[16] = 0;
    userdata->dsi.x[17] = 0;
    userdata->dsi.x[18] = 0;
    userdata->dsi.len = 19;

    me.codec = CODEC_TYPE_OPUS;
    me.name = &userdata->name;
    me.channels = source->channels;
    me.sample_rate = 48000;
    me.frame_len = userdata->framelen;
    me.padding = lookahead;
    me.roll_distance = -3840 / 960;
    me.sync_flag = 1;
    me.handle = userdata;
    me.set_keyframes = encoder_plugin_opus_set_keyframes;
    me.reset = encoder_plugin_opus_reset;
    me.dsi = &userdata->dsi;

    if( (r = dest->open(dest->handle,&me)) != 0) {
        LOG0("error opening muxer");
    }

    return r;
}


static int encoder_plugin_opus_flush(void* ud, const packet_receiver* dest) {
    int r = 0;
    encoder_plugin_opus_userdata* userdata = (encoder_plugin_opus_userdata*)ud;

    if(userdata->buffer.duration > 0) {
        if(userdata->buffer.duration < userdata->framelen) {
            if( (r = frame_fill(&userdata->buffer,userdata->framelen)) != 0) {
                LOG0("error filling frame buffer");
                return r;
            }
        }
        if( (r = opus_drain(userdata,dest)) != 0) return r;
    }

    return dest->flush(dest->handle);
}

static int encoder_plugin_opus_submit_frame(void* ud, const frame* frame, const packet_receiver* dest) {
    int r;
    encoder_plugin_opus_userdata* userdata = (encoder_plugin_opus_userdata*)ud;

    if( (r = frame_append(&userdata->buffer,frame)) != 0) {
        LOG1("error appending frame to internal buffer: %d",r);
        return r;
    }

    return opus_drain(userdata,dest);
}

static int encoder_plugin_opus_init(void) {
    return 0;
}

static void encoder_plugin_opus_deinit(void) {
    return;
}

const encoder_plugin encoder_plugin_opus = {
    { .a = 0, .len = 4, .x = (uint8_t*)"opus" },
    encoder_plugin_opus_init,
    encoder_plugin_opus_deinit,
    encoder_plugin_opus_create,
    encoder_plugin_opus_config,
    encoder_plugin_opus_open,
    encoder_plugin_opus_close,
    encoder_plugin_opus_submit_frame,
    encoder_plugin_opus_flush,
};
