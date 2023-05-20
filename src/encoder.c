#include "encoder.h"
#include <stdio.h>

void encoder_init(encoder* e) {
    e->userdata = NULL;
    e->plugin = NULL;
    e->packet_receiver = packet_receiver_zero;
}

void encoder_free(encoder* e) {
    if(e->userdata != NULL) {
        e->plugin->close(e->userdata);
    }
    e->userdata = NULL;
    e->plugin = NULL;
}

int encoder_create(encoder* e, const strbuf* name) {
    const encoder_plugin* plug;
    void* userdata;

    plug = encoder_plugin_get(name);
    if(plug == NULL) {
        fprintf(stderr,"[encoder] plugin \"%.*s\" not found\n",
          (int)name->len,(char *)name->x);
        return -1;
    }

    userdata = plug->create();
    if(userdata == NULL) {
        fprintf(stderr,"[encoder] error creating instance of \"%.*s\"\n",
          (int)name->len,(char *)name->x);
        return -1;
    }

    e->userdata = userdata;
    e->plugin = plug;

    return 0;
}

int encoder_open(encoder* e, const frame_source* source) {
    if(e->plugin == NULL || e->userdata == NULL) {
        fprintf(stderr,"[encoder] unable to open: plugin not selected\n");
        return -1;
    }
    ich_time_now(&e->ts);
    e->counter = 0;
    return e->plugin->open(e->userdata, source, &e->packet_receiver);
}

int encoder_config(const encoder* e, const strbuf* name, const strbuf* value) {
    return e->plugin->config(e->userdata,name,value);
}

int encoder_submit_frame(encoder* e, const frame* frame) {
    int r = e->plugin->submit_frame(e->userdata, frame, &e->packet_receiver);
    if(r == 0) {
        ich_time_now(&e->ts);
        e->counter++;
    }
    return r;
}

int encoder_flush(const encoder* e) {
    return e->plugin->flush(e->userdata, &e->packet_receiver);
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

    fprintf(stderr,"%.*s encoder: encodes=%zu last_encode=%4u-%02u-%02u %02u:%02u:%02u\n",
      (int)prefix->len,(const char*)prefix->x,
      in->counter,
      tm.year,tm.month,tm.day,
      tm.hour,tm.min,tm.sec);
}
