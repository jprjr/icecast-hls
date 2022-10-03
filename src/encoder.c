#include "encoder.h"
#include <stdio.h>

/* provide a default packet handler that just makes everything quit */
static int encoder_default_packet_handler(void* userdata, const packet* p) {
    (void)userdata;
    (void)p;
    fprintf(stderr,"[encoder] packet handler not set\n");
    return -1;
}

static int encoder_default_muxerconfig_handler(void* userdata, const muxerconfig* config) {
    (void)userdata;
    (void)config;
    fprintf(stderr,"[encoder] muxerconfig handler not set\n");
    return -1;
}

static int encoder_default_dsi_handler(void* userdata, const membuf* data) {
    (void)userdata;
    (void)data;
    fprintf(stderr,"[encoder] dsi handler not set\n");
    return -1;
}

int encoder_set_packet_handler(encoder* e, const packet_handler* p) {
    e->packet_handler = *p;
    return 0;
}

int encoder_set_muxerconfig_handler(encoder* e, const muxerconfig_handler* a) {
    e->muxerconfig_handler = *a;
    return 0;
}

void encoder_init(encoder* e) {
    e->userdata = NULL;
    e->plugin = NULL;
    e->packet_handler.cb = encoder_default_packet_handler;
    e->packet_handler.userdata = NULL;
    e->muxerconfig_handler.submit     = encoder_default_muxerconfig_handler;
    e->muxerconfig_handler.submit_dsi = encoder_default_dsi_handler;
    e->muxerconfig_handler.userdata   = NULL;
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

int encoder_open(const encoder* e, const audioconfig* c) {
    if(e->plugin == NULL || e->userdata == NULL) {
        fprintf(stderr,"[encoder] unable to open: plugin not selected\n");
        return -1;
    }
    return e->plugin->open(e->userdata,c, &e->muxerconfig_handler);
}

int encoder_config(const encoder* e, const strbuf* name, const strbuf* value) {
    return e->plugin->config(e->userdata,name,value);
}

int encoder_submit_frame(const encoder* e, const frame* frame) {
    return e->plugin->submit_frame(e->userdata, frame, &e->packet_handler);
}

int encoder_flush(const encoder* e) {
    return e->plugin->flush(e->userdata, &e->packet_handler);
}

int encoder_global_init(void) {
    return encoder_plugin_global_init();
}

void encoder_global_deinit(void) {
    return encoder_plugin_global_deinit();
}

