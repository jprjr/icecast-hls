#include "demuxer.h"
#include "demuxer_plugin.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int demuxer_default_tag_handler(void* userdata, const taglist* tags) {
    (void)userdata;
    (void)tags;
    fprintf(stderr,"[demuxer] tag handler not set\n");
    return -1;
}

void demuxer_init(demuxer* dem) {
    dem->userdata = NULL;
    dem->plugin = NULL;
    dem->packet_receiver = packet_receiver_zero;
    dem->tag_handler.cb = demuxer_default_tag_handler;
    dem->tag_handler.userdata = NULL;
}

void demuxer_free(demuxer* dem) {
    if(dem->userdata != NULL) {
        dem->plugin->close(dem->userdata);
        free(dem->userdata);
    }
    dem->userdata = NULL;
    dem->plugin = NULL;
}

int demuxer_create(demuxer* dem, const strbuf* name) {
    const demuxer_plugin* plug;
    void* userdata;

    plug = demuxer_plugin_get(name);
    if(plug == NULL) return -1;

    userdata = malloc(plug->size());
    if(userdata == NULL) return -1;

    dem->userdata = userdata;
    dem->plugin = plug;

    return dem->plugin->create(dem->userdata);
}


int demuxer_open(demuxer* dem, input* in) {
    if(dem->plugin == NULL || dem->userdata == NULL) {
        fprintf(stderr,"[demuxer] unable to open: no plugin selected\n");
        return -1;
    }
    ich_time_now(&dem->ts);
    dem->counter = 0;
    return dem->plugin->open(dem->userdata, in);
}

int demuxer_config(const demuxer* dem, const strbuf* name, const strbuf* value) {
    return dem->plugin->config(dem->userdata,name,value);
}

int demuxer_global_init(void) {
    return demuxer_plugin_global_init();
}

void demuxer_global_deinit(void) {
    demuxer_plugin_global_deinit();
}

int demuxer_run(demuxer* dem) {
    int r = dem->plugin->run(dem->userdata, &dem->tag_handler, &dem->packet_receiver);
    if(r == 0) {
        ich_time_now(&dem->ts);
        dem->counter++;
    }
    return r;
}

void demuxer_dump_counters(const demuxer* in, const strbuf* prefix) {
    ich_tm tm;
    ich_time_to_tm(&tm,&in->ts);

    fprintf(stderr,"%.*s demuxer: demuxes=%zu last_read=%4u-%02u-%02u %02u:%02u:%02u\n",
      (int)prefix->len,(const char*)prefix->x,
      in->counter,
      tm.year,tm.month,tm.day,
      tm.hour,tm.min,tm.sec);
}

