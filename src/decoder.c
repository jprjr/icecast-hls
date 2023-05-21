#include "decoder.h"
#include "decoder_plugin.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int decoder_default_tag_handler(void* userdata, const taglist* tags) {
    (void)userdata;
    (void)tags;
    fprintf(stderr,"[decoder] tag handler not set\n");
    return -1;
}

void decoder_init(decoder* dec) {
    dec->userdata = NULL;
    dec->plugin = NULL;
    dec->frame_receiver = frame_receiver_zero;
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


int decoder_open(decoder* dec, input* in) {
    if(dec->plugin == NULL || dec->userdata == NULL) {
        fprintf(stderr,"[decoder] unable to open: no plugin selected\n");
        return -1;
    }
    ich_time_now(&dec->ts);
    dec->counter = 0;
    return dec->plugin->open(dec->userdata, in, &dec->frame_receiver);
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

int decoder_run(decoder* dec) {
    int r = dec->plugin->decode(dec->userdata, &dec->tag_handler, &dec->frame_receiver);
    if(r == 0) {
        ich_time_now(&dec->ts);
        dec->counter++;
    }
    return r;
}

void decoder_dump_counters(const decoder* in, const strbuf* prefix) {
    ich_tm tm;
    ich_time_to_tm(&tm,&in->ts);

    fprintf(stderr,"%.*s decoder: decodes=%zu last_read=%4u-%02u-%02u %02u:%02u:%02u\n",
      (int)prefix->len,(const char*)prefix->x,
      in->counter,
      tm.year,tm.month,tm.day,
      tm.hour,tm.min,tm.sec);
}
