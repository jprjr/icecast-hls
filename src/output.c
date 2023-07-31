#include "output.h"
#include "output_plugin.h"

#include <stdio.h>
#include <stdlib.h>

#define LOG_PREFIX "[output]"
#include "logger.h"

void output_init(output* out) {
    out->userdata = NULL;
    out->plugin = NULL;
    out->opened = 0;
}

void output_free(output* out) {
    if(out->userdata != NULL) {
        logs_debug("closing");
        out->plugin->close(out->userdata);
        free(out->userdata);
    }
    out->userdata = NULL;
    out->plugin = NULL;
}

int output_create(output* out, const strbuf* name) {
    const output_plugin* plug;
    void* userdata;

    log_debug("loading %.*s plugin",
      (int)name->len,(const char *)name->x);

    plug = output_plugin_get(name);
    if(plug == NULL) {
        log_error("unable to find plugin %.*s",
          (int)name->len,(char *)name->x);
        return -1;
    }

    userdata = malloc(plug->size());
    if(userdata == NULL) {
        logs_fatal("uable to allocate plugin");
        return -1;
    }

    out->userdata = userdata;
    out->plugin = plug;

    return out->plugin->create(out->userdata);
}

int output_get_segment_info(const output* out, const segment_source_info* info, segment_params* params) {
    int r = -1;
    uint64_t t;
    if(out->plugin == NULL || out->userdata == NULL) {
        logs_error("plugin not selected");
        return r;
    }
    if( (r = out->plugin->get_segment_info(out->userdata,info,params)) != 0) return r;

    if(params->segment_length == 0) {
        /* plugin didn't set a length so we set
         * a default of 1s */
        params->segment_length = 1000;
    }

    if(params->packets_per_segment == 0 && info->frame_len != 0) {
        /* plugin didn't request a number of
         * packets per segment so we'll calculate
         * it here */
        t  = (uint64_t)params->segment_length;
        t *= (uint64_t)info->time_base;
        t /= (uint64_t)1000;
        t /= (uint64_t)info->frame_len;
        params->packets_per_segment = t;
    }

    return 0;
}

int output_open(output* out, const segment_source* source) {
    if(out->plugin == NULL || out->userdata == NULL) {
        logs_error("plugin not selected");
        return -1;
    }
    if(out->opened != 0) {
        logs_fatal("tried to re-open");
        return -1;
    }
    ich_time_now(&out->ts);
    out->counter = 0;
    out->opened = 1;

    log_debug("opening %.*s plugin",
      (int)out->plugin->name.len,
      (const char *)out->plugin->name.x);

    return out->plugin->open(out->userdata, source);
}

int output_config(const output* out, const strbuf* name, const strbuf* value) {
    log_debug("configuring plugin %.*s %.*s=%.*s",
      (int)out->plugin->name.len,
      (const char *)out->plugin->name.x,
      (int)name->len,
      (const char *)name->x,
      (int)value->len,
      (const char *)value->x);
    return out->plugin->config(out->userdata,name,value);
}

int output_set_time(const output* out, const ich_time* now) {
    return out->plugin->set_time(out->userdata,now);
}

int output_submit_segment(output* out, const segment* seg) {
    int r = out->plugin->submit_segment(out->userdata,seg);
    if(r == 0) {
        ich_time_now(&out->ts);
        out->counter++;
    }
    return r;
}

int output_submit_tags(const output* out, const taglist* tags) {
    return out->plugin->submit_tags(out->userdata,tags);
}

int output_submit_picture(const output* out, const picture* seg, picture* p) {
    return out->plugin->submit_picture(out->userdata,seg,p);
}

int output_flush(const output* out) {
    return out->plugin->flush(out->userdata);
}

int output_reset(const output* out) {
    return out->plugin->reset(out->userdata);
}

int output_global_init(void) {
    return output_plugin_global_init();
}

void output_global_deinit(void) {
    output_plugin_global_deinit();
}

void output_dump_counters(const output* in, const strbuf* prefix) {
    ich_tm tm;
    ich_time_to_tm(&tm,&in->ts);

    log_info("%.*s output: outputs=%zu last_output=%4u-%02u-%02u %02u:%02u:%02u",
      (int)prefix->len,(const char*)prefix->x,
      in->counter,
      tm.year,tm.month,tm.day,
      tm.hour,tm.min,tm.sec);
}
