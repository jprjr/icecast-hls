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

int packet_source_set_params_null(void* handle, const packet_source_params* params) {
    (void)handle;
    (void)params;
    fprintf(stderr,"[app error] packet_source set_params not set\n");
    abort();
    return -1;
}

int packet_source_set_params_ignore(void* handle, const packet_source_params* params) {
    (void)handle;
    (void)params;
    return 0;
}

const packet packet_zero = PACKET_ZERO;
const packet_receiver packet_receiver_zero = PACKET_RECEIVER_ZERO;
const packet_source packet_source_zero = PACKET_SOURCE_ZERO;
const packet_source_params packet_source_params_zero = PACKET_SOURCE_PARAMS_ZERO;
