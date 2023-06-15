#include "packet.h"
#include <stdio.h>
#include <stdlib.h>

void packet_init(packet* p) {
    membuf_init(&p->data);
    p->duration = 0;
    p->sync = 0;
    p->sample_rate = 0;
    p->pts = 0;
}

void packet_free(packet* p) {
    membuf_free(&p->data);
}

int packet_set_data(packet* p, const void* src, size_t len) {
    membuf_reset(&p->data);
    return membuf_append(&p->data,src,len);
}

int packet_copy(packet* dest, const packet* source) {
    dest->duration = source->duration;
    dest->sample_rate = source->sample_rate;
    dest->pts = source->pts;
    dest->sync = source->sync;
    return membuf_copy(&dest->data,&source->data);
}

int packet_receiver_open_null(void* handle, const packet_source* source) {
    (void)handle;
    (void)source;
    fprintf(stderr,"[app error] packet_receiver open not set\n");
    abort();
    return -1;
}

int packet_receiver_submit_dsi_null(void* handle, const membuf* dsi) {
    (void)handle;
    (void)dsi;
    fprintf(stderr,"[app error] packet_receiver submit_dsi not set\n");
    abort();
    return -1;
}

int packet_receiver_submit_packet_null(void* handle, const packet* packet) {
    (void)handle;
    (void)packet;
    fprintf(stderr,"[app error] packet_receiver submit_packet not set\n");
    abort();
    return -1;
}

int packet_receiver_flush_null(void* handle) {
    (void)handle;
    fprintf(stderr,"[app error] packet_receiver flush not set\n");
    abort();
    return -1;
}

int packet_source_set_keyframes_null(void* handle, unsigned int keyframes) {
    (void)handle;
    (void)keyframes;
    fprintf(stderr,"[app error] packet_source set_keyframes not set\n");
    abort();
    return -1;
}

int packet_source_reset_null(void* handle, void* dest, int (*cb)(void*, const packet*)) {
    (void)handle;
    (void)dest;
    (void)cb;
    fprintf(stderr,"[app error] packet_source reset not set\n");
    abort();
    return -1;
}

uint32_t packet_receiver_get_caps_null(void* handle) {
    (void)handle;
    fprintf(stderr,"[app error] packet_receiver get_caps not set\n");
    abort();
    return -1;
}

int packet_receiver_get_segment_info_null(const void* userdata, const packet_source_info* p, packet_source_params* i) {
    (void)userdata;
    (void)p;
    (void)i;
    fprintf(stderr,"[app error] packet_receiver get_segment_info not set\n");
    abort();
    return -1;
}

const packet packet_zero = PACKET_ZERO;
const packet_receiver packet_receiver_zero = PACKET_RECEIVER_ZERO;
const packet_source packet_source_zero = PACKET_SOURCE_ZERO;
const packet_source_params packet_source_params_zero = PACKET_SOURCE_PARAMS_ZERO;
