#include "filter.h"
#include "filter_plugin.h"
#include "frame.h"

#include <stdio.h>

void filter_init(filter* f) {
    f->userdata = NULL;
    f->plugin = NULL;
    f->frame_receiver = frame_receiver_zero;
}

void filter_free(filter* f) {
    if(f->userdata != NULL) {
        f->plugin->close(f->userdata);
    }
    f->userdata = NULL;
    f->plugin = NULL;
}

int filter_create(filter* f, const strbuf* name) {
    const filter_plugin* plug;
    void* userdata;

    plug = filter_plugin_get(name);
    if(plug == NULL) {
        fprintf(stderr,"[filter:create] unable to find plugin %.*s\n",
          (int)name->len,(char *)name->x);
        return -1;
    }

    userdata = plug->create();
    if(userdata == NULL) {
        fprintf(stderr,"[filter:create] unable to create instance of plugin %.*s\n",
          (int)name->len,(char *)name->x);
        return -1;
    }

    f->userdata = userdata;
    f->plugin = plug;

    return 0;
}

int filter_open(filter* f, const frame_source* source) {
    if(f->plugin == NULL || f->userdata == NULL) {
        fprintf(stderr,"[filter] unable to open: plugin not selected\n");
        return -1;
    }
    ich_time_now(&f->ts);
    f->counter = 0;
    return f->plugin->open(f->userdata,source, &f->frame_receiver);
}

int filter_config(const filter* f, const strbuf* name, const strbuf* value) {
    return f->plugin->config(f->userdata, name,value);
}

int filter_global_init(void) {
    return filter_plugin_global_init();
}

void filter_global_deinit(void) {
    return filter_plugin_global_deinit();
}

int filter_submit_frame(filter* f, const frame* frame) {
    int r = f->plugin->submit_frame(f->userdata, frame, &f->frame_receiver);
    if(r == 0) {
        ich_time_now(&f->ts);
        f->counter++;
    }
    return r;
}

int filter_flush(const filter* f) {
    return f->plugin->flush(f->userdata, &f->frame_receiver);
}

void filter_dump_counters(const filter* f, const strbuf* prefix) {
    ich_tm tm;
    ich_time_to_tm(&tm,&f->ts);

    fprintf(stderr,"%.*s filter: filters=%zu last_filter=%4u-%02u-%02u %02u:%02u:%02u\n",
      (int)prefix->len,(const char*)prefix->x,
      f->counter,
      tm.year,tm.month,tm.day,
      tm.hour,tm.min,tm.sec);
}
