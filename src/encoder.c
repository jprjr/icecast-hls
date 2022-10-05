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

int encoder_open(const encoder* e, const frame_source* source) {
    if(e->plugin == NULL || e->userdata == NULL) {
        fprintf(stderr,"[encoder] unable to open: plugin not selected\n");
        return -1;
    }
    return e->plugin->open(e->userdata, source, &e->packet_receiver);
}

int encoder_config(const encoder* e, const strbuf* name, const strbuf* value) {
    return e->plugin->config(e->userdata,name,value);
}

int encoder_submit_frame(const encoder* e, const frame* frame) {
    return e->plugin->submit_frame(e->userdata, frame, &e->packet_receiver);
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

