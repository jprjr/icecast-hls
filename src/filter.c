#include "filter.h"
#include "filter_plugin.h"
#include "frame.h"

#include <stdio.h>
#include <stdlib.h>

#define LOG_PREFIX "[filter]"
#include "logger.h"

void filter_init(filter* f) {
    f->userdata = NULL;
    f->plugin = NULL;
    f->frame_receiver = frame_receiver_zero;
    frame_init(&f->frame);
    f->frame_source = frame_source_zero;
    f->frame_source.packet_source = packet_source_zero;
    f->pts = 0;
}

void filter_free(filter* f) {
    if(f->userdata != NULL) {
        logs_debug("closing");
        f->plugin->close(f->userdata);
        free(f->userdata);
    }
    frame_free(&f->frame);
    f->userdata = NULL;
    f->plugin = NULL;
}

int filter_create(filter* f, const strbuf* name) {
    const filter_plugin* plug;
    void* userdata;

    log_debug("loading %.*s plugin",
      (int)name->len,(const char *)name->x);

    plug = filter_plugin_get(name);
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

    f->userdata = userdata;
    f->plugin = plug;

    return f->plugin->create(f->userdata);
}

static int filter_open_wrapper(void* ud, const frame_source* source) {
    filter* f = (filter *)ud;
    int r;

    /* if the filter is putting out a different channel count,
     * sample rate, or is binary), we'll want to call reset
     * on the receiver */

    switch(f->frame_source.format) {
        default: {
            /* if we're spitting out the same frame format
             * we don't need to reset + reopen the receiver */
            if(f->frame_source.channel_layout == source->channel_layout &&
               f->frame_source.sample_rate == source->sample_rate) return 0;

            if(f->frame_source.channel_layout != source->channel_layout) {
                log_debug("channel layout change, prev=0x%lx, new=0x%lx",
                  f->frame_source.channel_layout, source->channel_layout);
            }
            if(f->frame_source.sample_rate != source->sample_rate) {
                log_debug("sample rate change, prev=%u, new=%u",
                  f->frame_source.sample_rate, source->sample_rate);
            }
        }
        /* fall-through */
        case SAMPLEFMT_BINARY: {
            logs_info("change detected, flushing and resetting frame receiver");
            if( (r = f->frame_receiver.flush(f->frame_receiver.handle)) != 0) return r;
            if( (r = f->frame_receiver.reset(f->frame_receiver.handle)) != 0) return r;
            f->pts = 0;
        }
        /* fall-through */
        case SAMPLEFMT_UNKNOWN: {
            /* initial open, just save format for later */
            f->frame_source.format = source->format;
            f->frame_source.channel_layout = source->channel_layout;
            f->frame_source.sample_rate = source->sample_rate;
            break;
        }
    }

    return f->frame_receiver.open(f->frame_receiver.handle,source);
}

static int filter_submit_frame_wrapper(void* ud, const frame* frame) {
    filter* f = (filter *)ud;
    int r;

    if( (r = frame_copy(&f->frame,frame)) != 0) return r;

    f->frame.pts = f->pts;

    if( (r = f->frame_receiver.submit_frame(f->frame_receiver.handle, &f->frame)) != 0) return r;

    f->pts += f->frame.duration;
    return r;

}

int filter_open(filter* f, const frame_source* source) {
    frame_receiver receiver = FRAME_RECEIVER_ZERO;

    if(f->plugin == NULL || f->userdata == NULL) {
        logs_error("plugin not selected");
        return -1;
    }
    ich_time_now(&f->ts);
    f->counter = 0;

    receiver.handle = f;
    receiver.open = filter_open_wrapper;

    log_debug("opening %.*s plugin",
      (int)f->plugin->name->len,
      (const char *)f->plugin->name->x);

    return f->plugin->open(f->userdata,source, &receiver);
}

int filter_config(const filter* f, const strbuf* name, const strbuf* value) {
    log_debug("configuring plugin %.*s %.*s=%.*s",
      (int)f->plugin->name->len,
      (const char *)f->plugin->name->x,
      (int)name->len,
      (const char *)name->x,
      (int)value->len,
      (const char *)value->x);
    return f->plugin->config(f->userdata, name,value);
}

int filter_global_init(void) {
    return filter_plugin_global_init();
}

void filter_global_deinit(void) {
    return filter_plugin_global_deinit();
}

int filter_submit_frame(filter* f, const frame* frame) {
    int r;
    frame_receiver receiver = FRAME_RECEIVER_ZERO;

    receiver.handle = f;
    receiver.submit_frame = filter_submit_frame_wrapper;

    r = f->plugin->submit_frame(f->userdata, frame, &receiver);
    if(r == 0) {
        ich_time_now(&f->ts);
        f->counter++;
    }
    return r;
}

int filter_flush(const filter* f) {
    return f->plugin->flush(f->userdata, &f->frame_receiver);
}

int filter_reset(const filter* f) {
    return f->plugin->reset(f->userdata);
}

void filter_dump_counters(const filter* f, const strbuf* prefix) {
    ich_tm tm;
    ich_time_to_tm(&tm,&f->ts);

    log_info("%.*s filter: filters=%zu last_filter=%4u-%02u-%02u %02u:%02u:%02u",
      (int)prefix->len,(const char*)prefix->x,
      f->counter,
      tm.year,tm.month,tm.day,
      tm.hour,tm.min,tm.sec);
}
