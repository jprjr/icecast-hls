#include "avpacket_utils.h"
#include "ffmpeg-versions.h"

#if !ICH_AVCODEC_PACKET_MAKE_WRITABLE
static int av_packet_make_writable(AVPacket* out) {
    AVBufferRef *buf = NULL;
    int r;

    if(out->buf && av_buffer_is_writable(out->buf)) return 0;

    if( (r = av_buffer_realloc(&buf, out->size + AV_INPUT_BUFFER_PADDING_SIZE)) < 0) return r;
    memset(buf->data + out->size, 0, AV_INPUT_BUFFER_PADDING_SIZE);

    if(out->size) {
        memcpy(buf->data, out->data, out->size);
    }
    av_buffer_unref(&out->buf);
    out->buf = buf;
    out->data = buf->data;
    return 0;
}
#endif


int packet_to_avpacket(AVPacket* out, const packet* in) {
    out->data = in->data.x;
    out->size = in->data.len;

    out->duration = in->duration;
    out->flags = in->sync ? AV_PKT_FLAG_KEY : 0;
    out->pts   = (int64_t)in->pts;

#if ICH_AVCODEC_PACKET_HAS_TIME_BASE
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

#if ICH_AVCODEC_PACKET_HAS_TIME_BASE
    out->sample_rate = in->time_base.den;
#endif

    return 0;
}
