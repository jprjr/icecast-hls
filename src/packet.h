#ifndef PACKET_H
#define PACKET_H

#include "membuf.h"

struct packet {
    membuf data;
    size_t duration;
    uint8_t sync; /* 1 if the packet is a sync packet */
};

typedef struct packet packet;

typedef int (*packet_handler_callback)(void* userdata, const packet*);
typedef int (*packet_handler_flush_callback)(void* userdata);

struct packet_handler {
    packet_handler_callback cb;
    packet_handler_flush_callback flush;
    void* userdata;
};

typedef struct packet_handler packet_handler;

#ifdef __cplusplus
extern "C" {
#endif

void packet_init(packet*);
void packet_free(packet*);

int packet_set_data(packet*, const void* src, size_t len);

#ifdef __cplusplus
}
#endif

#endif
