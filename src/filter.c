#include "filter.h"
#include "filter_plugin.h"
#include "frame.h"

#include <stdio.h>

/* provide a default frame handler that just makes everything quit */
static int filter_default_frame_handler(void* userdata, const frame *frame) {
    (void)userdata;
    (void)frame;
    fprintf(stderr,"[filter] frame handler not set\n");
    return -1;
}

static int filter_default_flush_handler(void* userdata) {
    (void)userdata;
    fprintf(stderr,"[filter] flush handler not set\n");
    return -1;
}

static int filter_default_audioconfig_handler(void* userdata, const audioconfig* config) {
    (void)userdata;
    (void)config;
    fprintf(stderr,"[filter] audioconfig handler not set\n");
    return -1;
}

int filter_set_frame_handler(filter* f, const frame_handler* handler) {
    f->frame_handler = *handler;
    return 0;
}

int filter_set_audioconfig_handler(filter* f, const audioconfig_handler* handler) {
    f->audioconfig_handler = *handler;
    return 0;
}

void filter_init(filter* f) {
    f->userdata = NULL;
    f->plugin = NULL;
    f->frame_handler.cb    = filter_default_frame_handler;
    f->frame_handler.flush = filter_default_flush_handler;
    f->frame_handler.userdata = NULL;
    f->audioconfig_handler.open = filter_default_audioconfig_handler;
    f->audioconfig_handler.userdata = NULL;
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

int filter_open(const filter* f, const audioconfig* config) {
    if(f->plugin == NULL || f->userdata == NULL) {
        fprintf(stderr,"[filter] unable to open: plugin not selected\n");
        return -1;
    }
    return f->plugin->open(f->userdata, config, &f->audioconfig_handler);
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

int filter_submit_frame(const filter* f, const frame* frame) {
    return f->plugin->submit_frame(f->userdata, frame, &f->frame_handler);
}

int filter_flush(const filter* f) {
    return f->plugin->flush(f->userdata, &f->frame_handler);
}

