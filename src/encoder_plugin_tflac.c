#include "encoder_plugin.h"

#include "tflac.h"
#include "packet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#define LOG_PREFIX "[encoder:tflac]"
#include "logger.h"

#define LOGS(s,a) log_error(s,(int)(a).len,(char *)(a).x)
#define TRY0(exp, act) if( (r = (exp)) != 0 ) { act; goto cleanup; }

struct plugin_userdata {
    tflac t;
    packet packet;
    frame buffer;
    frame scaled;
    membuf t_memory;
    packet_source me;
    unsigned int blocksize;
    unsigned int bps;
    TFLAC_CHANNEL_MODE channel_mode;
    uint8_t enable_constant_subframe;
    uint8_t enable_fixed_subframe;
};

typedef struct plugin_userdata plugin_userdata;

static int plugin_create(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    tflac_init(&userdata->t);
    packet_init(&userdata->packet);
    frame_init(&userdata->buffer);
    frame_init(&userdata->scaled);
    membuf_init(&userdata->t_memory);
    userdata->me = packet_source_zero;

    userdata->blocksize = 0;
    userdata->bps = 0;
    userdata->channel_mode = TFLAC_CHANNEL_INDEPENDENT;
    userdata->enable_constant_subframe = 0;
    userdata->enable_fixed_subframe = 1;
    return 0;
}

static int plugin_init(void) {
    tflac_detect_cpu();
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static size_t plugin_size(void) {
    return sizeof(plugin_userdata);
};

static int plugin_config(void* ud, const strbuf* key, const strbuf* value) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(strbuf_equals_cstr(key, "blocksize") ||
       strbuf_equals_cstr(key, "block-size") ||
       strbuf_equals_cstr(key, "block size")) {
        userdata->blocksize = strbuf_strtoul(value,10);
        if(errno != 0) {
            log_error("error parsing blocksize value %.*s",
              (int)value->len,(char *)value->x);
            return -1;
        }
        return 0;
    }

    if(strbuf_equals_cstr(key, "bps") ||
       strbuf_equals_cstr(key, "bitdepth")) {
        userdata->bps = strbuf_strtoul(value,10);
        if(errno != 0) {
            log_error("error parsing bps value %.*s",
              (int)value->len,(char *)value->x);
            return -1;
        }
        if(userdata->bps < 4 || userdata->bps > 32) {
            log_error("bps %.*s out of range",
              (int)value->len,(char *)value->x);
            return -1;
        }
        return 0;
    }

    if(strbuf_equals_cstr(key, "constant subframe") ||
       strbuf_equals_cstr(key, "constant-subframe") ||
       strbuf_equals_cstr(key, "constant_subframe")) {
        if(strbuf_truthy(value)) {
            userdata->enable_constant_subframe = 1;
            return 0;
        }
        if(strbuf_falsey(value)) {
            userdata->enable_constant_subframe = 0;
            return 0;
        }
        LOGS("invalid value for constant-subframe: %.*s",(*value));
        return -1;
    }

    if(strbuf_equals_cstr(key, "fixed subframe") ||
       strbuf_equals_cstr(key, "fixed-subframe") ||
       strbuf_equals_cstr(key, "fixed_subframe")) {
        if(strbuf_truthy(value)) {
            userdata->enable_fixed_subframe = 1;
            return 0;
        }
        if(strbuf_falsey(value)) {
            userdata->enable_fixed_subframe = 0;
            return 0;
        }
        LOGS("invalid value for fixed-subframe: %.*s",(*value));
        return -1;
    }

    if(strbuf_equals_cstr(key, "channel mode") ||
       strbuf_equals_cstr(key, "channel-mode") ||
       strbuf_equals_cstr(key, "channel_mode")) {
        if(strbuf_equals_cstr(value, "independent")) {
            userdata->channel_mode = TFLAC_CHANNEL_INDEPENDENT;
            return 0;
        }
        if(strbuf_equals_cstr(value, "left-side") ||
           strbuf_equals_cstr(value, "left side") ||
           strbuf_equals_cstr(value, "left_side")) {
            userdata->channel_mode = TFLAC_CHANNEL_LEFT_SIDE;
            return 0;
        }
        if(strbuf_equals_cstr(value, "side-right") ||
           strbuf_equals_cstr(value, "side right") ||
           strbuf_equals_cstr(value, "side_right")) {
            userdata->channel_mode = TFLAC_CHANNEL_SIDE_RIGHT;
            return 0;
        }
        if(strbuf_equals_cstr(value, "mid-side") ||
           strbuf_equals_cstr(value, "mid side") ||
           strbuf_equals_cstr(value, "mid_side")) {
            userdata->channel_mode = TFLAC_CHANNEL_MID_SIDE;
            return 0;
        }
        LOGS("invalid value for channel-mode: %.*s",(*value));
        return -1;
    }

    LOGS("unknown key: %.*s",*key);
    return -1;
}

static int plugin_reset(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    membuf_reset(&userdata->t_memory);
    strbuf_reset(&userdata->me.dsi);
    return 0;
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    packet_free(&userdata->packet);
    frame_free(&userdata->buffer);
    frame_free(&userdata->scaled);
    membuf_free(&userdata->t_memory);
    strbuf_free(&userdata->me.dsi);
}

static int plugin_open(void* ud, const frame_source* source, const packet_receiver* dest) {
    int r = 0;
    tflac_u32 mem_used;
    plugin_userdata* userdata = (plugin_userdata*) ud;

    if(userdata->blocksize != 0) {
        if(userdata->blocksize * source->sample_rate % 1000) {
            userdata->blocksize = 0;
        } else {
            userdata->blocksize = userdata->blocksize * source->sample_rate / 1000;
        }
    }

    if(userdata->blocksize == 0) {
        switch(source->sample_rate) {
            case 8000: userdata->blocksize  = 960; /* 120ms */ break;
            case 16000: userdata->blocksize = 960; /* 60ms */ break;
            case 22050: userdata->blocksize = 882; /* 40ms */ break;
            case 24000: userdata->blocksize = 960; /* 40ms */ break;
            case 32000: userdata->blocksize = 1280; /* 40ms */ break;
            case 44100: userdata->blocksize = 882;  break;
            case 48000: userdata->blocksize = 960; break;
            case 88200: userdata->blocksize = 1764; break;
            case 96000: userdata->blocksize = 1920; break;
            case 176400: userdata->blocksize = 3528; break;
            case 192000: userdata->blocksize = 3840; break;
            default: userdata->blocksize = 4608; break;
        }
    }
    if(userdata->bps == 0) userdata->bps = 16;

    userdata->t.blocksize  = userdata->blocksize;
    userdata->t.samplerate = source->sample_rate;
    userdata->t.bitdepth   = userdata->bps;
    userdata->t.channels   = channel_count(source->channel_layout);

    userdata->t.enable_constant_subframe = userdata->enable_constant_subframe;
    userdata->t.enable_fixed_subframe = userdata->enable_fixed_subframe;
    userdata->t.channel_mode = userdata->channel_mode;
    userdata->t.enable_md5 = 0;

    userdata->packet.sample_rate = userdata->t.samplerate;

    userdata->buffer.format = SAMPLEFMT_S32;
    userdata->buffer.channels = userdata->t.channels;
    userdata->buffer.duration = 0;
    userdata->buffer.sample_rate = userdata->t.samplerate;

    userdata->scaled.format = SAMPLEFMT_S32;
    userdata->scaled.channels = userdata->t.channels;
    userdata->scaled.duration = userdata->t.blocksize;
    userdata->scaled.sample_rate = userdata->t.samplerate;

    TRY0(membuf_ready(&userdata->t_memory,tflac_size_memory(userdata->blocksize)), logs_fatal("error allocating tflac memory"));
    TRY0(membuf_ready(&userdata->packet.data,tflac_size_frame(userdata->t.blocksize, userdata->t.channels, userdata->t.bitdepth)), logs_fatal("error allocating packet buffer"));

    TRY0(tflac_validate(&userdata->t, userdata->t_memory.x, userdata->t_memory.a), logs_fatal("error validating tflac encoder"));

    TRY0(frame_ready(&userdata->buffer), logs_fatal("error allocating samples buffer"));
    TRY0(frame_buffer(&userdata->scaled), logs_fatal("error allocating scaled samples buffer"));

    tflac_encode_streaminfo(&userdata->t, 1, userdata->packet.data.x, userdata->packet.data.a, &mem_used);

    userdata->me.codec = CODEC_TYPE_FLAC;
    userdata->me.dsi.x = &userdata->packet.data.x[4];
    userdata->me.dsi.len = mem_used - 4;
    userdata->me.dsi.a = 0;

    TRY0(dest->open(dest->handle, &userdata->me), logs_error("error opening muxer"));

    r = 0;
    cleanup:
    return r;
}

static int plugin_encode_frame(plugin_userdata* userdata, const packet_receiver* dest, unsigned int blocksize) {
    int r = 0;
    size_t i = 0;
    size_t len = 0;
    tflac_s32 scale = 1 << (32 - userdata->t.bitdepth);
    int32_t* scaled = NULL;
    int32_t* samples = NULL;
    tflac_u32 mem_used = 0;

    samples = (int32_t*) frame_get_channel_samples(&userdata->buffer, 0);
    scaled  = (int32_t*) frame_get_channel_samples(&userdata->scaled, 0);

    len = ((size_t)userdata->buffer.channels) * ((size_t)blocksize);
    while(i < len) {
        scaled[i] = samples[i] / scale;
        i++;
    }

    TRY0(tflac_encode_s32i(&userdata->t, blocksize, scaled, userdata->packet.data.x, userdata->packet.data.a, &mem_used), logs_error("error encoding frame"));

    userdata->packet.data.len = (size_t)mem_used;
    userdata->packet.duration = blocksize;

    TRY0(dest->submit_packet(dest->handle, &userdata->packet), logs_error("error sending packet to muxer"));

    userdata->packet.pts += blocksize;
    frame_trim(&userdata->buffer, blocksize);

    r = 0;
    cleanup:
    return r;
}

static int plugin_drain(plugin_userdata* userdata, const packet_receiver* dest) {
    int r = 0;

    while(userdata->buffer.duration >= userdata->t.blocksize) {
        if( (r = plugin_encode_frame(userdata, dest, userdata->t.blocksize)) != 0) break;
    }

    return r;
}

static int plugin_flush(void* ud, const packet_receiver* dest) {
    int r;

    plugin_userdata* userdata = (plugin_userdata*)ud;

    TRY0(plugin_drain(userdata, dest), logs_error("error draining frames"));
    if(userdata->buffer.duration > 0) {
        r = plugin_encode_frame(userdata, dest, userdata->buffer.duration);
    }

    cleanup:
    return r;
}

static int plugin_submit_frame(void* ud, const frame* frame, const packet_receiver* dest) {
    int r;

    plugin_userdata* userdata = (plugin_userdata*)ud;

    r = 0;

    TRY0(frame_append(&userdata->buffer, frame), logs_error("error appending frame to buffer"));

    r = plugin_drain(userdata, dest);
    cleanup:
    return r;
}

static STRBUF_CONST(plugin_name,"tflac");

const encoder_plugin encoder_plugin_tflac = {
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
