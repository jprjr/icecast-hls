#include "encoder_plugin.h"
#include "muxer_caps.h"

#include "avframe_utils.h"
#include "avpacket_utils.h"

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/log.h>

#include "ffmpeg-versions.h"

#include <stdlib.h>
#include <inttypes.h>

#define LOG_PREFIX "[encoder:avcodec]"
#include "logger.h"

#define TRY0(exp, act) if( (r = (exp)) != 0 ) { act; goto cleanup; }
#define TRY(exp, act) if(!(exp)) { act; r = -1; goto cleanup; }

#if !(ICH_AVCODEC_PACKETALLOC)
static AVPacket* av_packet_alloc(void) {
    AVPacket *p = (AVPacket*)av_mallocz(sizeof(AVPacket));
    if(p == NULL) return NULL;
    av_init_packet(p);
    return p;
}

static void av_packet_free(AVPacket** p) {
    av_free_packet(*p);
    av_free(*p);
    *p = NULL;
}
#endif

static inline int ctx_frame_size(const AVCodecContext *ctx) {
    return ctx->frame_size;
}

static inline int ctx_profile(const AVCodecContext *ctx) {
    return ctx->profile;
}

static inline uint8_t * ctx_extradata(const AVCodecContext *ctx) {
    return ctx->extradata;
}

static inline int ctx_extradata_size(const AVCodecContext *ctx) {
    return ctx->extradata_size;
}

static inline int ctx_initial_padding(const AVCodecContext *ctx) {
    return ctx->initial_padding;
}

static inline const AVCodecDescriptor* ctx_codec_descriptor(const AVCodecContext *ctx) {
    return ctx->codec_descriptor;
}

static inline void ctx_time_base(AVCodecContext *ctx, AVRational time_base) {
    ctx->time_base = time_base;
}

static inline void ctx_sample_rate(AVCodecContext *ctx, int sample_rate) {
    ctx->sample_rate = sample_rate;
}

static inline void ctx_sample_fmt(AVCodecContext *ctx, enum AVSampleFormat sample_fmt) {
    ctx->sample_fmt = sample_fmt;
}

static inline void ctx_channel_layout(AVCodecContext *ctx, uint64_t mask) {
#if ICH_AVUTIL_CHANNEL_LAYOUT
    av_channel_layout_from_mask(&ctx->ch_layout, mask);
#else
    ctx->channel_layout = mask;
#endif
}

static inline void ctx_flags(AVCodecContext *ctx, int flags) {
    ctx->flags |= flags;
}

#if !(ICH_AVCODEC_SENDPACKET)
/* we're using the encode_audio2-style API, we'll mimc the new API */
typedef struct ICH_AVCodecContext {
    AVCodecContext *codec_ctx;
    AVPacket *packet;
    int flag;
} ICH_AVCodecContext;

static void ich_avcodec_free_context(ICH_AVCodecContext **ctx) {
    if( (*ctx)->codec_ctx != NULL ) {
        avcodec_free_context(& ((*ctx)->codec_ctx) );
    }

    if( (*ctx)->packet != NULL) {
        av_packet_free(& ((*ctx)->packet) );
    }

    av_free( (*ctx) );
    *ctx = NULL;
}

static ICH_AVCodecContext* ich_avcodec_alloc_context3(const AVCodec *codec) {
    ICH_AVCodecContext *ctx = av_mallocz(sizeof(ICH_AVCodecContext));
    if(ctx == NULL) return NULL;

    ctx->packet = av_packet_alloc();
    if(ctx->packet == NULL) {
        ich_avcodec_free_context(&ctx);
        return NULL;
    }

    ctx->codec_ctx = avcodec_alloc_context3(codec);
    if(ctx->codec_ctx == NULL) {
        ich_avcodec_free_context(&ctx);
        return NULL;
    }

    return ctx;
}

static int ich_avcodec_open2(ICH_AVCodecContext *ctx, const AVCodec *codec, AVDictionary **options) {
    return avcodec_open2(ctx->codec_ctx, codec, options);
}

static int ich_avcodec_send_frame(ICH_AVCodecContext *ctx, const AVFrame *frame) {
    int r;
    int got;

    av_packet_unref(ctx->packet);
    if( (r = avcodec_encode_audio2(ctx->codec_ctx, ctx->packet, frame, &got)) < 0) {
        return r;
    }

    ctx->flag = 0;
    if(got) {
        ctx->flag = 1; /* we have a packet */
    }
    if(frame == NULL) {
        ctx->flag += 2;
    }
    /* flag values:
     * 0 = return averror(eagain)
     * 1 = return the packet
     * 2 = return averror_eof
     * 3 = return the packet and call encode_audio2 again
     */
    return 0;
}

static int ich_avcodec_receive_packet(ICH_AVCodecContext *ctx, AVPacket *pkt) {
    switch(ctx->flag) {
        case 0: return AVERROR(EAGAIN);
        case 1: {
            av_packet_move_ref(pkt,ctx->packet);
            ctx->flag = 0;
            return 0;
        }
        case 2: return AVERROR_EOF;
        default: break;
    }
    av_packet_move_ref(pkt,ctx->packet);
    return ich_avcodec_send_frame(ctx, NULL);
}

static inline int ich_ctx_frame_size(const ICH_AVCodecContext *ctx) {
    return ctx_frame_size(ctx->codec_ctx);
}

static inline int ich_ctx_profile(const ICH_AVCodecContext *ctx) {
    return ctx_profile(ctx->codec_ctx);
}

static inline uint8_t * ich_ctx_extradata(const ICH_AVCodecContext *ctx) {
    return ctx_extradata(ctx->codec_ctx);
}

static inline int ich_ctx_extradata_size(const ICH_AVCodecContext *ctx) {
    return ctx_extradata_size(ctx->codec_ctx);
}

static inline int ich_ctx_initial_padding(const ICH_AVCodecContext *ctx) {
    return ctx_initial_padding(ctx->codec_ctx);
}

static inline const AVCodecDescriptor* ich_ctx_codec_descriptor(const ICH_AVCodecContext *ctx) {
    return ctx_codec_descriptor(ctx->codec_ctx);
}

static inline void ich_ctx_time_base(ICH_AVCodecContext *ctx, AVRational time_base) {
    ctx_time_base(ctx->codec_ctx, time_base);
}

static inline void ich_ctx_sample_rate(ICH_AVCodecContext *ctx, int sample_rate) {
    ctx_sample_rate(ctx->codec_ctx, sample_rate);
}

static inline void ich_ctx_sample_fmt(ICH_AVCodecContext *ctx, enum AVSampleFormat sample_fmt) {
    ctx_sample_fmt(ctx->codec_ctx, sample_fmt);
}

static inline void ich_ctx_channel_layout(ICH_AVCodecContext *ctx, uint64_t mask) {
    ctx_channel_layout(ctx->codec_ctx, mask);
}

static inline void ich_ctx_flags(ICH_AVCodecContext *ctx, int flags) {
    ctx_flags(ctx->codec_ctx, flags);
}

#define AVCodecContext ICH_AVCodecContext
#define avcodec_send_frame ich_avcodec_send_frame
#define avcodec_receive_packet ich_avcodec_receive_packet

#define avcodec_free_context ich_avcodec_free_context
#define avcodec_alloc_context3 ich_avcodec_alloc_context3
#define avcodec_open2 ich_avcodec_open2

#define ctx_frame_size ich_ctx_frame_size
#define ctx_profile ich_ctx_profile
#define ctx_extradata ich_ctx_extradata
#define ctx_extradata_size ich_ctx_extradata_size
#define ctx_initial_padding ich_ctx_initial_padding
#define ctx_codec_descriptor ich_ctx_codec_descriptor

#define ctx_time_base ich_ctx_time_base
#define ctx_sample_rate ich_ctx_sample_rate
#define ctx_sample_fmt ich_ctx_sample_fmt
#define ctx_channel_layout ich_ctx_channel_layout
#define ctx_flags ich_ctx_flags

#endif

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
#if ICH_AVCODEC_SAMPLE_FMTS
    fmt = codec->sample_fmts;
#else
    if(avcodec_get_supported_config(NULL, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT,
        0, (const void**)&fmt, NULL) < 0) return AV_SAMPLE_FMT_NONE;
#endif
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

    while( (av = avcodec_receive_packet(userdata->ctx, userdata->avpacket)) >= 0) {
        if(userdata->avpacket->duration == 0) {
            if(!flushing) {
                logs_fatal("received a packet with zero duration");
                av = AVERROR_EXTERNAL;
                goto complete;
            }
            av = AVERROR_EOF;
            goto complete;
        }

        if(avpacket_to_packet(&userdata->packet,userdata->avpacket) != 0) {
          logs_fatal("unable to convert packet");
          av = AVERROR_EXTERNAL;
          goto complete;
        }
        userdata->packet.sample_rate = userdata->sample_rate;

        if(dest->submit_packet(dest->handle, &userdata->packet) != 0) {
          logs_error("unable to send packet");
          av = AVERROR_EXTERNAL;
          goto complete;
        }
    }

    complete:
    return av;
}

static int open_encoder(plugin_userdata* userdata) {
    int r = -1;
    AVDictionary *opts = NULL;
    AVDictionaryEntry *t = NULL;
    AVRational time_base;

    TRY( (userdata->ctx = avcodec_alloc_context3(userdata->codec)) != NULL,
        logs_fatal("out of memory"));

    time_base.num = 1;
    time_base.den = userdata->sample_rate;

    ctx_time_base(userdata->ctx, time_base);
    ctx_sample_rate(userdata->ctx, userdata->sample_rate);
    ctx_sample_fmt(userdata->ctx, userdata->sample_fmt);
    ctx_channel_layout(userdata->ctx, userdata->channel_layout);

    if(userdata->codec_config != NULL) {
#if ICH_AVUTIL_DICT_COPY_INT
        TRY(av_dict_copy(&opts,userdata->codec_config,0) == 0,
            logs_fatal("error copying codec_config"));
#else
        av_dict_copy(&opts,userdata->codec_config,0);
#endif
    }

    if(userdata->muxer_caps & MUXER_CAP_GLOBAL_HEADERS) {
        ctx_flags(userdata->ctx, AV_CODEC_FLAG_GLOBAL_HEADER);
    }

    TRY(avcodec_open2(userdata->ctx, userdata->codec, &opts) >= 0,
        logs_fatal("unable to open codec context"));

    if(opts != NULL) {
        while( (t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX)) != NULL) {
            log_error("warning: unused option %s=%s",
              t->key, t->value);
        }
    }

    r = 0;

    cleanup:
    if(opts != NULL) av_dict_free(&opts);
    return r;
}

static void plugin_avlog(void* ud, int level, const char* fmt, va_list ap) {
    (void)ud;
    enum LOG_LEVEL l;
    switch(level) {
        case AV_LOG_ERROR: l = LOG_ERROR; break;
        case AV_LOG_WARNING: l = LOG_WARN; break;
        case AV_LOG_INFO: l = LOG_INFO; break;
        case AV_LOG_VERBOSE: l = LOG_DEBUG; break;
        case AV_LOG_DEBUG: l = LOG_DEBUG; break;
        case AV_LOG_TRACE: l = LOG_TRACE; break;
        default: l = LOG_FATAL; break;
    }
    vlogger_log(l, __FILE__, __LINE__, fmt, ap);
}

static int plugin_init(void) {
#if ICH_AVCODEC_REGISTER_ALL
    avcodec_register_all();
#endif
    /* this might conflict with the avformat demuxer
     * or avcodec decoder but it really doesn't matter */
    av_log_set_callback(plugin_avlog);
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
        av_packet_free(&userdata->avpacket);
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
        TRY0(strbuf_copy(&tmp1,value),logs_fatal("out of memory"));
        TRY0(strbuf_term(&tmp1),logs_fatal("out of memory"));

        TRY( (userdata->codec = avcodec_find_encoder_by_name((char *)tmp1.x)) != NULL,
          log_error("unable to find codec %s",(char*)tmp1.x));
        goto cleanup;
    }

    TRY0(strbuf_copy(&tmp1,key),logs_fatal("out of memory"));
    TRY0(strbuf_term(&tmp1),logs_fatal("out of memory"));
    TRY0(strbuf_copy(&tmp2,value),logs_fatal("out of memory"));
    TRY0(strbuf_term(&tmp2),logs_fatal("out of memory"));

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
            logs_error("no encoder specified and unable to find a default"));
    }

    userdata->sample_rate    = source->sample_rate;
    userdata->channel_layout = source->channel_layout;
    TRY( (userdata->sample_fmt = find_best_format(userdata->codec,samplefmt_to_avsampleformat(source->format))) != AV_SAMPLE_FMT_NONE,
        logs_error("unable to find a suitable sample format"));

    TRY(open_encoder(userdata) == 0, logs_error("error opening encoder"));

    if( ctx_extradata_size(userdata->ctx) > 0) {
        TRY0(membuf_append(&userdata->me.dsi, ctx_extradata(userdata->ctx), ctx_extradata_size(userdata->ctx)),
        logs_fatal("out of memory"));
    }

    switch(userdata->codec->id) {
        case AV_CODEC_ID_AAC: {
            userdata->me.codec = CODEC_TYPE_AAC;
            userdata->me.profile = ctx_profile(userdata->ctx) + 1;
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
            userdata->me.roll_distance = -3840 / ctx_frame_size(userdata->ctx);
            break;
        }

        default: {
            TRY(0, logs_error("unsupported codec type"));
        }
    }

    userdata->buffer.format = avsampleformat_to_samplefmt(userdata->sample_fmt);
    userdata->buffer.channels = channel_count(userdata->channel_layout);
    userdata->buffer.duration = 0;
    userdata->buffer.sample_rate = source->sample_rate;

    if( (r = frame_ready(&userdata->buffer)) != 0) return r;

    userdata->me.handle = userdata;
    userdata->me.channel_layout = source->channel_layout;
    userdata->me.sample_rate = source->sample_rate;
    userdata->me.frame_len = ctx_frame_size(userdata->ctx);
    userdata->me.sync_flag = ctx_codec_descriptor(userdata->ctx)->props & AV_CODEC_PROP_INTRA_ONLY;
    userdata->me.padding = ctx_initial_padding(userdata->ctx);

    if(userdata->me.frame_len == 0) userdata->me.frame_len = 1024;

    TRY( (userdata->avframe = av_frame_alloc()) != NULL,   logs_fatal("out of memory"));
    TRY( (userdata->avpacket = av_packet_alloc()) != NULL, logs_fatal("out of memory"));

    TRY0(dest->open(dest->handle, &userdata->me), logs_error("error configuring muxer"));

    cleanup:
    return r;

}

static int plugin_drain(plugin_userdata* userdata, const packet_receiver* dest, unsigned int duration) {
    int r;
    int av;
    char averrbuf[128];

    r = 0;
    while(userdata->buffer.duration >= (unsigned int)duration) {
        TRY0(frame_to_avframe(userdata->avframe,&userdata->buffer,duration,userdata->channel_layout),
          logs_fatal("unable to convert frame"));
        frame_trim(&userdata->buffer,duration);

        TRY( (av = avcodec_send_frame(userdata->ctx,userdata->avframe)) >= 0,
          av_strerror(av, averrbuf, sizeof(averrbuf));
          log_error("unable to send frame: %s",averrbuf));

        TRY( (av = drain_packets(userdata,dest, 0)) == AVERROR(EAGAIN),
          av_strerror(av, averrbuf, sizeof(averrbuf));
          log_error("frame: error receiving packet: %s",averrbuf));
        r = 0;
    }

    cleanup:
    return r;

}

static int plugin_submit_frame(void* ud, const frame* frame, const packet_receiver* dest) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if( (r = frame_append(&userdata->buffer,frame)) != 0) {
        log_error("error appending frame to internal buffer: %d",r);
        return r;
    }

    return plugin_drain(userdata, dest, (unsigned int)ctx_frame_size(userdata->ctx));
}


static int plugin_flush(void* ud, const packet_receiver* dest) {
    int r;
    int av;
    char averrbuf[128];
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(userdata->ctx == NULL) return 0;

    if( (r = plugin_drain(userdata, dest, (unsigned int)ctx_frame_size(userdata->ctx))) != 0) {
        return r;
    }
    if(userdata->buffer.duration > 0) {
        if( (r = plugin_drain(userdata, dest, userdata->buffer.duration)) != 0) {
            return r;
        }
    }

    TRY( (av = avcodec_send_frame(userdata->ctx,NULL)) >= 0,
      av_strerror(av, averrbuf, sizeof(averrbuf));
      log_error("unable to flush encoder: %s",averrbuf));

    TRY( (av = drain_packets(userdata,dest,1)) == AVERROR_EOF,
      av_strerror(av, averrbuf, sizeof(averrbuf));
      log_error("flush: error receiving packet: %s",averrbuf));
    r = 0;

    cleanup:

    return r;
}

const encoder_plugin encoder_plugin_avcodec = {
    &plugin_name,
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

