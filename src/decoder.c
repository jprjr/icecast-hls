#include "decoder.h"
#include "decoder_plugin.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int decoder_set_frame_handler(decoder* dec, const frame_handler* f) {
    dec->frame_handler = *f;
    return 0;
}

int decoder_set_tag_handler(decoder* dec, const tag_handler* t) {
    dec->tag_handler = *t;
    return 0;
}

static int decoder_default_frame_handler(void* userdata, const frame *frame) {
    (void)userdata;
    (void)frame;
    fprintf(stderr,"[decoder] frame handler not set\n");
    return -1;
}

static int decoder_default_flush_handler(void* userdata) {
    (void)userdata;
    fprintf(stderr,"[decoder] flush handler not set\n");
    return -1;
}

static int decoder_default_tag_handler(void* userdata, const taglist* tags) {
    (void)userdata;
    (void)tags;
    fprintf(stderr,"[decoder] tag handler not set\n");
    return -1;
}

void decoder_init(decoder* dec) {
    dec->userdata = NULL;
    dec->plugin = NULL;
    dec->frame_handler.cb = decoder_default_frame_handler;
    dec->frame_handler.flush = decoder_default_flush_handler;
    dec->frame_handler.userdata = NULL;
    dec->tag_handler.cb = decoder_default_tag_handler;
    dec->tag_handler.userdata = NULL;
}

void decoder_free(decoder* dec) {
    if(dec->userdata != NULL) {
        dec->plugin->close(dec->userdata);
    }
    dec->userdata = NULL;
    dec->plugin = NULL;
}

int decoder_create(decoder* dec, const strbuf* name) {
    const decoder_plugin* plug;
    void* userdata;

    plug = decoder_plugin_get(name);
    if(plug == NULL) return -1;

    userdata = plug->create();
    if(userdata == NULL) return -1;

    dec->userdata = userdata;
    dec->plugin = plug;

    return 0;
}


int decoder_open(const decoder* dec, const input* in, const audioconfig_handler* ahdlr) {
    if(dec->plugin == NULL || dec->userdata == NULL) {
        fprintf(stderr,"[decoder] unable to open: no plugin selected\n");
        return -1;
    }
    return dec->plugin->open(dec->userdata, in, ahdlr);
}

int decoder_config(const decoder* dec, const strbuf* name, const strbuf* value) {
    return dec->plugin->config(dec->userdata,name,value);
}

int decoder_global_init(void) {
    return decoder_plugin_global_init();
}

void decoder_global_deinit(void) {
    decoder_plugin_global_deinit();
}

int decoder_decode(const decoder* dec) {
    return dec->plugin->decode(dec->userdata, &dec->tag_handler, &dec->frame_handler);
}
