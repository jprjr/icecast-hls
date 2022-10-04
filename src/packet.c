#include "packet.h"

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
