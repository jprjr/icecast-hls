#include "avpacket_utils.h"

int packet_to_avpacket(AVPacket* out, const packet* in) {
    out->data = in->data.x;
    out->size = in->data.len;

    out->duration = in->duration;
    out->flags = in->sync ? AV_PKT_FLAG_KEY : 0;
    out->pts   = (int64_t)in->pts;

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57,17,100)
    out->time_base.den = in->sample_rate;
    out->time_base.num = 1;
#endif
    if(av_packet_make_writable(out) != 0) return -1;

    return 0;
}

int avpacket_to_packet(packet* out, const AVPacket* in) {
    int r;

    packet_reset(out);

    if( (r = membuf_ready(&out->data, in->size)) != 0) return r;
    memcpy(&out->data.x[0],in->data,in->size);
    out->data.len = in->size;

    out->duration    = in->duration;
    out->sync        = in->flags &  AV_PKT_FLAG_KEY;
    out->pts         = (uint64_t)in->pts;

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57,17,100)
    out->sample_rate = in->time_base.den;
#endif

    return 0;
}
