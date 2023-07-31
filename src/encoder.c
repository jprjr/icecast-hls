#include "encoder.h"
#include <stdio.h>
#include <stdlib.h>

#include "muxer_caps.h"

#define LOG_PREFIX "[encoder]"
#include "logger.h"

void encoder_init(encoder* e) {
    e->userdata = NULL;
    e->plugin = NULL;
    e->codec = CODEC_TYPE_UNKNOWN;
    e->packet_receiver = packet_receiver_zero;
    e->frame_source = frame_source_zero;
    e->frame_source.packet_source = packet_source_zero;
    e->prev_frame_source = frame_source_zero;
}

void encoder_free(encoder* e) {
    if(e->userdata != NULL) {
        logs_debug("closing");
        e->plugin->close(e->userdata);
        free(e->userdata);
    }
    e->userdata = NULL;
    e->plugin = NULL;
}

int encoder_create(encoder* e, const strbuf* name) {
    const encoder_plugin* plug;
    void* userdata;

    log_debug("loading %.*s plugin",
      (int)name->len,(const char *)name->x);

    plug = encoder_plugin_get(name);
    if(plug == NULL) {
        log_error("unable to find plugin %.*s",
          (int)name->len,(char *)name->x);
        return -1;
    }

    userdata = malloc(plug->size());
    if(userdata == NULL) {
        logs_fatal("unable to allocate plugin");
        return -1;
    }

    e->userdata = userdata;
    e->plugin = plug;

    return e->plugin->create(e->userdata);
}

static uint32_t encoder_get_caps_wrapper(void* ud) {
    encoder* e = (encoder *)ud;
    return e->packet_receiver.get_caps(e->packet_receiver.handle);
}

static int encoder_get_segment_info_wrapper(const void* ud, const packet_source_info* i, packet_source_params* p) {
    const encoder* e = (encoder *)ud;
    return e->packet_receiver.get_segment_info(e->packet_receiver.handle,i,p);
}

static int encoder_open_wrapper(void* ud, const packet_source* source) {
    encoder* e = (encoder *)ud;
    int r;

    /* whenever we-re re-opening an encoder we need to call flush + reset */
    switch(e->codec) {
        default: /* re-open */ {
            logs_info("change detected, flushing and resetting packet receiver");
            if( (r = e->packet_receiver.flush(e->packet_receiver.handle)) != 0) return r;
            if( (r = e->packet_receiver.reset(e->packet_receiver.handle)) != 0) return r;
        }
        /* fall-through */
        case CODEC_TYPE_UNKNOWN: /* initial open */ {
            e->codec = source->codec;
            break;
        }
    }

    return e->packet_receiver.open(e->packet_receiver.handle, source);
}

int encoder_open(encoder* e, const frame_source* source) {
    int r;
    packet_receiver receiver = PACKET_RECEIVER_ZERO;

    if(e->plugin == NULL || e->userdata == NULL) {
        logs_error("plugin not selected");
        return -1;
    }
    ich_time_now(&e->ts);
    e->counter = 0;

    if( (r = frame_source_copy(&e->prev_frame_source, source)) != 0) return r;

    receiver.handle = e;
    receiver.open = encoder_open_wrapper;
    receiver.get_caps = encoder_get_caps_wrapper;
    receiver.get_segment_info = encoder_get_segment_info_wrapper;

    log_debug("opening %.*s plugin",
      (int)e->plugin->name.len,
      (const char *)e->plugin->name.x);

    return e->plugin->open(e->userdata, source, &receiver);
}

int encoder_config(const encoder* e, const strbuf* name, const strbuf* value) {
    log_debug("configuring plugin %.*s %.*s=%.*s",
      (int)e->plugin->name.len,
      (const char *)e->plugin->name.x,
      (int)name->len,
      (const char *)name->x,
      (int)value->len,
      (const char *)value->x);
    return e->plugin->config(e->userdata,name,value);
}

int encoder_submit_frame(encoder* e, const frame* frame) {
    int r;
    r = e->plugin->submit_frame(e->userdata, frame, &e->packet_receiver);

    if(r == 0) {
        ich_time_now(&e->ts);
        e->counter++;
    }
    return r;
}

int encoder_submit_tags(encoder* e, const taglist* tags) {
    int r;
    uint32_t caps = e->packet_receiver.get_caps(e->packet_receiver.handle);

    if(caps & MUXER_CAP_TAGS_RESET) { /* we're sending to ogg and need to reset the encoder state */
        if( (r = frame_source_copy(&e->frame_source, &e->prev_frame_source)) != 0) return r;
        if( (r = encoder_flush(e)) != 0) return r;
        if( (r = encoder_reset(e)) != 0) return r;
        if( (r = encoder_open(e, &e->frame_source)) != 0) return r;
    }

    return e->packet_receiver.submit_tags(e->packet_receiver.handle, tags);
}

int encoder_flush(const encoder* e) {
    return e->plugin->flush(e->userdata, &e->packet_receiver);
}

int encoder_reset(const encoder* e) {
    return e->plugin->reset(e->userdata);
}

int encoder_global_init(void) {
    return encoder_plugin_global_init();
}

void encoder_global_deinit(void) {
    return encoder_plugin_global_deinit();
}

void encoder_dump_counters(const encoder* in, const strbuf* prefix) {
    ich_tm tm;
    ich_time_to_tm(&tm,&in->ts);

    log_info("%.*s encoder: encodes=%zu last_encode=%4u-%02u-%02u %02u:%02u:%02u",
      (int)prefix->len,(const char*)prefix->x,
      in->counter,
      tm.year,tm.month,tm.day,
      tm.hour,tm.min,tm.sec);
}
