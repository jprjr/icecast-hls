#include "packet.h"
#include <stdio.h>
#include <stdlib.h>

void packet_reset(packet* p) {
    p->duration = 0;
    p->sync = 0;
    p->sample_rate = 0;
    p->sync = 0;
    membuf_reset(&p->data);
}

void packet_init(packet* p) {
    membuf_init(&p->data);
    packet_reset(p);
}

void packet_free(packet* p) {
    membuf_free(&p->data);
}

void packet_source_reset(packet_source* s) {
    membuf_reset(&s->dsi);
    s->handle = NULL;
    s->name = NULL;
    s->codec = CODEC_TYPE_UNKNOWN;
    s->profile = 0;
    s->channel_layout = 0;
    s->sample_rate = 0;
    s->frame_len = 0;
    s->bit_rate = 0;
    s->sync_flag = 0;
    s->padding = 0;
    s->roll_distance = 0;
    s->roll_type = 0;
    if(s->priv != NULL) {
        s->priv_free(s->priv);
        s->priv = NULL;
    }
    s->priv_free = NULL;
    s->priv_copy = NULL;
}

void packet_source_free(packet_source *s) {
    packet_source_reset(s);
    membuf_free(&s->dsi);
    if(s->priv != NULL) {
        s->priv_free(s->priv);
    }
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

int packet_source_copy(packet_source* dest, const packet_source* source) {
    dest->handle = source->handle;
    dest->name = source->name;
    dest->codec = source->codec;
    dest->profile = source->profile;
    dest->channel_layout = source->channel_layout;
    dest->sample_rate = source->sample_rate;
    dest->frame_len = source->frame_len;
    dest->sync_flag = source->sync_flag;
    dest->padding = source->padding;
    dest->roll_distance = source->roll_distance;
    dest->roll_type = source->roll_type;
    dest->bit_rate = source->bit_rate;
    if(source->priv != NULL) {
        if( (dest->priv = source->priv_copy(source->priv)) == NULL) return -1;
        dest->priv_copy = source->priv_copy;
        dest->priv_free = source->priv_free;
    }

    membuf_reset(&dest->dsi);
    return membuf_copy(&dest->dsi, &source->dsi);
}

int packet_receiver_open_null(void* handle, const packet_source* source) {
    (void)handle;
    (void)source;
    fprintf(stderr,"[app error] packet_receiver open not set\n");
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

int packet_receiver_submit_tags_null(void* handle, const taglist* tags) {
    (void)handle;
    (void)tags;
    fprintf(stderr,"[app error] tags_receiver submit_tags not set\n");
    abort();
    return -1;
}

int packet_receiver_flush_null(void* handle) {
    (void)handle;
    fprintf(stderr,"[app error] packet_receiver flush not set\n");
    abort();
    return -1;
}

int packet_receiver_reset_null(void* handle) {
    (void)handle;
    fprintf(stderr,"[app error] packet_receiver reset not set\n");
    abort();
    return -1;
}

int packet_receiver_close_null(void* handle) {
    (void)handle;
    fprintf(stderr,"[app error] packet_receiver close not set\n");
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
