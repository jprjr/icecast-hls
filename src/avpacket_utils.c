#include "avpacket_utils.h"

int avpacket_to_packet(packet* out, const AVPacket* in) {
    packet_free(out);

    out->data.x      = in->data;
    out->data.len    = in->size;
    out->duration    = in->duration;
    out->sync        = in->flags &  AV_PKT_FLAG_KEY;

    out->pts         = (uint64_t)in->pts;
    out->sample_rate = in->time_base.den;

    return 0;
}
