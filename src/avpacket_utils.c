#include "avpacket_utils.h"

int avpacket_to_packet(packet* out, const AVPacket* in) {
    packet_free(out);

    out->data.x      = in->data;
    out->data.len    = in->size;
    out->duration    = in->duration;
    out->sync        = in->flags &  AV_PKT_FLAG_KEY;

    out->pts         = (uint64_t)in->pts;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57,17,100)
    out->sample_rate = in->time_base.den;
#endif

    return 0;
}
