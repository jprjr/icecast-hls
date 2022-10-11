#include "avframe_utils.h"

#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

samplefmt avsampleformat_to_samplefmt(enum AVSampleFormat f) {
    switch(f) {
        case AV_SAMPLE_FMT_U8:   return SAMPLEFMT_U8;
        case AV_SAMPLE_FMT_U8P:  return SAMPLEFMT_U8P;
        case AV_SAMPLE_FMT_S16:  return SAMPLEFMT_S16;
        case AV_SAMPLE_FMT_S16P: return SAMPLEFMT_S16P;
        case AV_SAMPLE_FMT_S32:  return SAMPLEFMT_S32;
        case AV_SAMPLE_FMT_S32P: return SAMPLEFMT_S32P;
        case AV_SAMPLE_FMT_S64:  return SAMPLEFMT_S64;
        case AV_SAMPLE_FMT_S64P: return SAMPLEFMT_S64P;
        case AV_SAMPLE_FMT_FLT:  return SAMPLEFMT_FLOAT;
        case AV_SAMPLE_FMT_FLTP: return SAMPLEFMT_FLOATP;
        case AV_SAMPLE_FMT_DBL:  return SAMPLEFMT_DOUBLE;
        case AV_SAMPLE_FMT_DBLP: return SAMPLEFMT_DOUBLEP;
        default: break;
    }
    return SAMPLEFMT_UNKNOWN;
}

enum AVSampleFormat samplefmt_to_avsampleformat(samplefmt f) {
    switch(f) {
        case SAMPLEFMT_U8:   return AV_SAMPLE_FMT_U8;
        case SAMPLEFMT_U8P:  return AV_SAMPLE_FMT_U8P;
        case SAMPLEFMT_S16:  return AV_SAMPLE_FMT_S16;
        case SAMPLEFMT_S16P: return AV_SAMPLE_FMT_S16P;
        case SAMPLEFMT_S32:  return AV_SAMPLE_FMT_S32;
        case SAMPLEFMT_S32P: return AV_SAMPLE_FMT_S32P;
        case SAMPLEFMT_S64:  return AV_SAMPLE_FMT_S64;
        case SAMPLEFMT_S64P: return AV_SAMPLE_FMT_S64P;
        case SAMPLEFMT_FLOAT:  return AV_SAMPLE_FMT_FLT;
        case SAMPLEFMT_FLOATP: return AV_SAMPLE_FMT_FLTP;
        case SAMPLEFMT_DOUBLE:  return AV_SAMPLE_FMT_DBL;
        case SAMPLEFMT_DOUBLEP: return AV_SAMPLE_FMT_DBLP;
        default: break;
    }
    return AV_SAMPLE_FMT_NONE;
}

int frame_to_avframe(AVFrame* out, const frame* in) {
    int r;
    size_t i = 0;
    void* samples;
    char errmsg[128];

    av_frame_unref(out);

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57,17,100)
    out->time_base.num = 1;
    out->time_base.den = in->sample_rate;
#endif
    out->sample_rate = in->sample_rate;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57,28,100)
    av_channel_layout_default(&out->ch_layout,in->channels);
#else
    out->channel_layout = av_get_default_channel_layout(in->channels);
#endif
    out->nb_samples = in->duration;
    out->pts = in->pts;
    out->pkt_dts = in->pts;
    out->format = samplefmt_to_avsampleformat(in->format);

    if( (r = av_frame_get_buffer(out,0)) < 0) {
        av_strerror(r,errmsg,sizeof(errmsg));
        fprintf(stderr,"error allocating AVFrame buffer: %s\n",errmsg);
        return -1;
    }

    if(samplefmt_is_planar(in->format)) {
        for(i=0;i<in->channels;i++) {
            samples = frame_get_channel_samples(in,i);
            memcpy(out->data[i],samples,samplefmt_size(in->format) * in->duration);
        }
    } else {
        samples = frame_get_channel_samples(in,0);
        memcpy(out->data[0],samples,in->channels * samplefmt_size(in->format) * in->duration);
    }

    return 0;
}

int avframe_to_frame(frame* out, const AVFrame* in) {
    int r;
    size_t i;
    void* samples;

    out->duration = in->nb_samples;
    out->format = avsampleformat_to_samplefmt(in->format);
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57,28,100)
    out->channels = in->ch_layout.nb_channels;
#else
    out->channels = av_get_channel_layout_nb_channels(in->channel_layout);
#endif
    out->pts = in->pts;
    out->sample_rate = in->sample_rate;

    if( (r = frame_buffer(out)) != 0) return r;

    if(samplefmt_is_planar(out->format)) {
        for(i=0;i<out->channels;i++) {
            samples = frame_get_channel_samples(out,i);
            memcpy(samples,in->data[i],samplefmt_size(out->format) * out->duration);
        }
    } else {
        samples = frame_get_channel_samples(out,0);
        memcpy(samples,in->data[0],out->channels * samplefmt_size(out->format) * out->duration);
    }

    return 0;
}


