#include "input.h"
#include "input_plugin.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LOG_PREFIX "[input]"
#include "logger.h"

static int default_tag_handler(void* userdata, const taglist* tags) {
    (void)userdata;
    (void)tags;
    logs_error("tag handler not set");
    return -1;
}

void input_init(input* in) {
    in->userdata = NULL;
    in->plugin = NULL;
    in->tag_handler.cb = default_tag_handler;
    in->tag_handler.userdata = NULL;
}

void input_free(input* in) {
    if(in->userdata != NULL) {
        logs_debug("closing");
        in->plugin->close(in->userdata);
        free(in->userdata);
    }
    in->userdata = NULL;
    in->plugin = NULL;
}

int input_create(input* in, const strbuf* name) {
    const input_plugin* plug;
    void* userdata;

    log_debug("loading %.*s plugin",
      (int)name->len,(const char *)name->x);

    plug = input_plugin_get(name);
    if(plug == NULL) {
        log_error("unable to find %.*s plugin",
          (int)name->len,(const char*)name->x);
        return -1;
    }

    userdata = malloc(plug->size());
    if(userdata == NULL) {
        logs_fatal("unable to allocate plugin");
        return -1;
    }

    in->userdata = userdata;
    in->plugin = plug;

    return in->plugin->create(in->userdata);
}

int input_open(input* in) {
    if(in->plugin == NULL || in->userdata == NULL) {
        logs_error("plugin not selected");
        return -1;
    }

    in->counter = 0;
    ich_time_now(&in->ts);

    log_debug("opening %.*s plugin",
      (int)in->plugin->name->len,
      (const char *)in->plugin->name->x);
    return in->plugin->open(in->userdata);
}

size_t input_read(input* in, void* dest, size_t len) {
    size_t r = in->plugin->read(in->userdata,dest,len, &in->tag_handler);
    if(r) {
        ich_time_now(&in->ts);
        in->counter++;
    }
    return r;
}

int input_config(const input* in, const strbuf* name, const strbuf* value) {
    log_debug("configuring plugin %.*s %.*s=%.*s",
      (int)in->plugin->name->len,
      (const char *)in->plugin->name->x,
      (int)name->len,
      (const char *)name->x,
      (int)value->len,
      (const char *)value->x);
    return in->plugin->config(in->userdata,name,value);
}

int input_global_init(void) {
    return input_plugin_global_init();
}

void input_global_deinit(void) {
    input_plugin_global_deinit();
}

void input_dump_counters(const input* in, const strbuf* prefix) {
    ich_tm tm;
    ich_time_to_tm(&tm,&in->ts);

    log_debug("%.*s input: reads=%zu last_read=%4u-%02u-%02u %02u:%02u:%02u",
      (int)prefix->len,(const char*)prefix->x,
      in->counter,
      tm.year,tm.month,tm.day,
      tm.hour,tm.min,tm.sec);
}

