#include "encoder_plugin.h"
#include "muxer_caps.h"

#include "avframe_utils.h"
#include "avpacket_utils.h"

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>

#include <stdlib.h>

#define LOG0(fmt) fprintf(stderr, "[encoder:avcodec] " fmt "\n")
#define LOG1(fmt,a) fprintf(stderr, "[encoder:avcodec] " fmt "\n", (a))
#define LOG2(fmt,a,b) fprintf(stderr, "[encoder:avcodec] " fmt "\n", (a), (b))

#define TRY0(exp, act) if( (r = (exp)) != 0 ) { act; goto cleanup; }
#define TRY(exp, act) if(!(exp)) { act; r = -1; goto cleanup; }

static STRBUF_CONST(plugin_name,"avcodec");

struct plugin_userdata {
    const AVCodec* codec;
    AVCodecContext* ctx;
    AVDictionary* codec_config;
    AVFrame* avframe;
    AVPacket* avpacket;
    packet packet;
    frame buffer;

    unsigned int sample_rate;
    uint64_t channel_layout;
    enum AVSampleFormat sample_fmt;
    uint32_t muxer_caps;
    packet_source me;
};
typedef struct plugin_userdata plugin_userdata;

static enum AVSampleFormat find_best_format(const AVCodec *codec, enum AVSampleFormat srcFmt) {
    const enum AVSampleFormat *fmt = NULL;
    enum AVSampleFormat outFmt = AV_SAMPLE_FMT_NONE;

    /* find the appropriate output format for the codec */
    fmt = codec->sample_fmts;
    while(fmt != NULL && *fmt != AV_SAMPLE_FMT_NONE) {
        if(*fmt == srcFmt) {
            outFmt = *fmt;
            break;
        } else if(*fmt == av_get_planar_sample_fmt(srcFmt)) {
            outFmt = *fmt;
            break;
        } else if(*fmt == av_get_packed_sample_fmt(srcFmt)) {
            outFmt = *fmt;
            break;
        }
        /* pick the highest-res input format available */
        else if(av_get_packed_sample_fmt(*fmt) > outFmt) {
            outFmt = *fmt;
        }
        fmt++;
    }

    return outFmt;
}

static int drain_packets(plugin_userdata* userdata, const packet_receiver* dest, int flushing) {
    int av;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57,64,0) /* ffmpeg-3.2 */
    int got;
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,64,0) /* ffmpeg-3.2 */
    while( (av = avcodec_receive_packet(userdata->ctx, userdata->avpacket)) >= 0) {
#else
    while( (av = avcodec_encode_audio2(userdata->ctx, userdata->avpacket, NULL, &got)) >= 0 && got) {
#endif
        if(userdata->avpacket->duration == 0) {
            if(!flushing) {
                LOG0("error, received a packet with zero duration");
                av = AVERROR_EXTERNAL;
                goto complete;
            }
            av = AVERROR_EOF;
            goto complete;
        }

        if(avpacket_to_packet(&userdata->packet,userdata->avpacket) != 0) {
          LOG0("unable to convert packet");
          av = AVERROR_EXTERNAL;
          goto complete;
        }
        userdata->packet.sample_rate = userdata->sample_rate;

        if(dest->submit_packet(dest->handle, &userdata->packet) != 0) {
          LOG0("unable to send packet");
          av = AVERROR_EXTERNAL;
          goto complete;
        }
    }

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57,64,0) /* ffmpeg-3.2 */
    if(av == 0) av = AVERROR_EOF;
#endif

    complete:
    return av;
}

static int open_encoder(plugin_userdata* userdata) {
    int r = -1;
    AVDictionary *opts = NULL;
    AVDictionaryEntry *t = NULL;

    TRY( (userdata->ctx = avcodec_alloc_context3(userdata->codec)) != NULL,
        LOG0("out of memory"));

    userdata->ctx->time_base.num = 1;
    userdata->ctx->time_base.den = userdata->sample_rate;
    userdata->ctx->sample_rate   = userdata->sample_rate;
    userdata->ctx->sample_fmt    = userdata->sample_fmt;

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57,28,100)
    av_channel_layout_from_mask(&userdata->ctx->ch_layout,userdata->channel_layout);
#else
    userdata->ctx->channel_layout = userdata->channel_layout;
#endif

    if(userdata->codec_config != NULL) {
        TRY(av_dict_copy(&opts,userdata->codec_config,0) == 0,
            LOG0("error copying codec_config"));
    }

    if(userdata->muxer_caps & MUXER_CAP_GLOBAL_HEADERS) {
        userdata->ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    TRY(avcodec_open2(userdata->ctx, userdata->codec, &opts) >= 0,
        LOG0("unable to open codec context"));

    if(opts != NULL) {
        while( (t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX)) != NULL) {
            LOG2("warning: unused option %s=%s",
              t->key, t->value);
        }
    }

    r = 0;

    cleanup:
    if(opts != NULL) av_dict_free(&opts);
    return r;
}

static int plugin_init(void) {
#if LIBAVCODEC_VERSION_MAJOR < 58
    avcodec_register_all();
#endif
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static size_t plugin_size(void) {
    return sizeof(plugin_userdata);
}

static int plugin_create(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    userdata->codec = NULL;
    userdata->ctx = NULL;
    userdata->avframe = NULL;
    userdata->avpacket = NULL;
    userdata->codec_config = NULL;
    frame_init(&userdata->buffer);
    frame_init(&userdata->buffer);
    packet_init(&userdata->packet);
    userdata->me = packet_source_zero;

    return 0;
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    if(userdata->avframe != NULL) av_frame_free(&userdata->avframe);
    if(userdata->avpacket != NULL) {
#if LIBAVCODEC_VERSION_MAJOR >= 57
        av_packet_free(&userdata->avpacket);
#else
        av_free_packet(userdata->avpacket);
        av_free(userdata->avpacket);
#endif
    }
    if(userdata->ctx != NULL) avcodec_free_context(&userdata->ctx);
    if(userdata->codec_config != NULL) av_dict_free(&userdata->codec_config);
    frame_free(&userdata->buffer);
    packet_free(&userdata->packet);
    strbuf_free(&userdata->me.dsi);

    return;
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* value) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r = 0;
    strbuf tmp1;
    strbuf tmp2;

    strbuf_init(&tmp1);
    strbuf_init(&tmp2);

    if(strbuf_equals_cstr(key,"c") || strbuf_equals_cstr(key,"codec")) {
        TRY0(strbuf_copy(&tmp1,value),LOG0("out of memory"));
        TRY0(strbuf_term(&tmp1),LOG0("out of memory"));

        TRY( (userdata->codec = avcodec_find_encoder_by_name((char *)tmp1.x)) != NULL,
          LOG1("unable to find codec %s",(char*)tmp1.x));
        goto cleanup;
    }

    TRY0(strbuf_copy(&tmp1,key),LOG0("out of memory"));
    TRY0(strbuf_term(&tmp1),LOG0("out of memory"));
    TRY0(strbuf_copy(&tmp2,value),LOG0("out of memory"));
    TRY0(strbuf_term(&tmp2),LOG0("out of memory"));

    av_dict_set(&userdata->codec_config,(char *)tmp1.x,(char*)tmp2.x,0);

    cleanup:
    strbuf_free(&tmp1);
    strbuf_free(&tmp2);
    return r;
}

static int plugin_reset(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(userdata->ctx != NULL) avcodec_free_context(&userdata->ctx);
    strbuf_reset(&userdata->me.dsi);

    return 0;
}

static int plugin_open(void* ud, const frame_source* source, const packet_receiver *dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;

    userdata->muxer_caps = dest->get_caps(dest->handle);

    if(userdata->codec == NULL) {
        TRY( (userdata->codec = avcodec_find_encoder_by_name("aac")) != NULL,
            LOG0("no encoder specified and unable to find a default"));
    }

    userdata->sample_rate    = source->sample_rate;
    userdata->channel_layout = source->channel_layout;
    TRY( (userdata->sample_fmt = find_best_format(userdata->codec,samplefmt_to_avsampleformat(source->format))) != AV_SAMPLE_FMT_NONE,
        LOG0("unable to find a suitable sample format"));

    TRY(open_encoder(userdata) == 0, LOG0("error opening encoder"));

    if(userdata->ctx->extradata_size > 0) {
        TRY0(membuf_append(&userdata->me.dsi, userdata->ctx->extradata, userdata->ctx->extradata_size),
        LOG0("out of memory"));
    }

    switch(userdata->codec->id) {
        case AV_CODEC_ID_AAC: {
            userdata->me.codec = CODEC_TYPE_AAC;
            userdata->me.profile = userdata->ctx->profile + 1;
            userdata->me.roll_distance = -1;
            break;
        }

        case AV_CODEC_ID_ALAC: {
            userdata->me.codec = CODEC_TYPE_ALAC;
            break;
        }

        case AV_CODEC_ID_FLAC: {
            userdata->me.codec = CODEC_TYPE_FLAC;
            break;
        }

        case AV_CODEC_ID_MP3: {
            userdata->me.codec = CODEC_TYPE_MP3;
            break;
        }

        case AV_CODEC_ID_AC3: {
            userdata->me.codec = CODEC_TYPE_AC3;
            break;
        }

        case AV_CODEC_ID_EAC3: {
            userdata->me.codec = CODEC_TYPE_EAC3;
            break;
        }

        case AV_CODEC_ID_OPUS: {
            userdata->me.codec = CODEC_TYPE_OPUS;
            /* opus specs states you need (at least) 80ms of preroll,
             * which is 3840 samples @48kHz */
            userdata->me.roll_distance = -3840 / userdata->ctx->frame_size;
            break;
        }

        default: {
            TRY(0, LOG0("unsupported codec type"));
        }
    }

    userdata->buffer.format = avsampleformat_to_samplefmt(find_best_format(userdata->codec,samplefmt_to_avsampleformat(source->format)));
    userdata->buffer.channels = channel_count(source->channel_layout);
    userdata->buffer.duration = 0;
    userdata->buffer.sample_rate = source->sample_rate;

    if( (r = frame_ready(&userdata->buffer)) != 0) return r;

    userdata->me.handle = userdata;
    userdata->me.channel_layout = source->channel_layout;
    userdata->me.sample_rate = source->sample_rate;
    userdata->me.frame_len = userdata->ctx->frame_size;
    userdata->me.sync_flag = userdata->ctx->codec_descriptor->props & AV_CODEC_PROP_INTRA_ONLY;
    userdata->me.padding = userdata->ctx->initial_padding;

    if(userdata->me.frame_len == 0) userdata->me.frame_len = 1024;

    TRY( (userdata->avframe = av_frame_alloc()) != NULL, LOG0("out of memory"));
#if LIBAVCODEC_VERSION_MAJOR >= 57
    TRY( (userdata->avpacket = av_packet_alloc()) != NULL, LOG0("out of memory"));
#else
    TRY( (userdata->avpacket = av_mallocz(sizeof(AVPacket))) != NULL, LOG0("out of memory"));
    av_init_packet(userdata->avpacket);
#endif

    TRY0(dest->open(dest->handle, &userdata->me), LOG0("error configuring muxer"));

    cleanup:
    return r;

}

static int plugin_drain(plugin_userdata* userdata, const packet_receiver* dest, unsigned int duration) {
    int r;
    int av;
    char averrbuf[128];
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57,64,0) /* ffmpeg-3.2 */
    int got;
#endif

    r = 0;
    while(userdata->buffer.duration >= (unsigned int)duration) {
        TRY0(frame_to_avframe(userdata->avframe,&userdata->buffer,duration),
          LOG0("unable to convert frame"));
        frame_trim(&userdata->buffer,duration);

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,64,0) /* ffmpeg-3.2 */
        TRY( (av = avcodec_send_frame(userdata->ctx,userdata->avframe)) >= 0,
          av_strerror(av, averrbuf, sizeof(averrbuf));
          LOG1("unable to send frame: %s",averrbuf));

        TRY( (av = drain_packets(userdata,dest, 0)) == AVERROR(EAGAIN),
          av_strerror(av, averrbuf, sizeof(averrbuf));
          LOG1("frame: error receiving packet: %s",averrbuf));
        r = 0;
#else
        TRY( (av = avcodec_encode_audio2(userdata->ctx, userdata->avpacket, userdata->avframe, &got)) >= 0,
          av_strerror(av, averrbuf, sizeof(averrbuf));
          LOG1("error in avcodec_encode_audio2: %s",averrbuf));
        if(got) {
            if(userdata->avpacket->duration == 0) {
                LOG0("error, received a packet with zero duration");
                return AVERROR_EXTERNAL;
            }
            if(avpacket_to_packet(&userdata->packet,userdata->avpacket) != 0) {
              LOG0("unable to convert packet");
              return AVERROR_EXTERNAL;
            }
            userdata->packet.sample_rate = userdata->sample_rate;
            userdata->packet.sample_group = 1;

            if(dest->submit_packet(dest->handle, &userdata->packet) != 0) {
              LOG0("unable to send packet");
              return AVERROR_EXTERNAL;
            }
        }
        r = 0;
#endif
    }

    cleanup:
    return r;

}

static int plugin_submit_frame(void* ud, const frame* frame, const packet_receiver* dest) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if( (r = frame_append(&userdata->buffer,frame)) != 0) {
        LOG1("error appending frame to internal buffer: %d",r);
        return r;
    }

    return plugin_drain(userdata, dest, (unsigned int)userdata->ctx->frame_size);
}


static int plugin_flush(void* ud, const packet_receiver* dest) {
    int r;
    int av;
    char averrbuf[128];
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(userdata->ctx == NULL) return 0;

    if( (r = plugin_drain(userdata, dest, (unsigned int)userdata->ctx->frame_size)) != 0) {
        return r;
    }
    if(userdata->buffer.duration > 0) {
        if( (r = plugin_drain(userdata, dest, userdata->buffer.duration)) != 0) {
            return r;
        }
    }

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,64,0) /* ffmpeg-3.2 */
    TRY( (av = avcodec_send_frame(userdata->ctx,NULL)) >= 0,
      av_strerror(av, averrbuf, sizeof(averrbuf));
      LOG1("unable to flush encoder: %s",averrbuf));
#endif

    TRY( (av = drain_packets(userdata,dest,1)) == AVERROR_EOF,
      av_strerror(av, averrbuf, sizeof(averrbuf));
      LOG1("flush: error receiving packet: %s",averrbuf));
    r = 0;

    cleanup:

    return r;
}

const encoder_plugin encoder_plugin_avcodec = {
    plugin_name,
    plugin_size,
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_submit_frame,
    plugin_flush,
    plugin_reset,
};

