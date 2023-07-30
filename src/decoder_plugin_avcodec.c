#include "decoder_plugin_avcodec.h"

#include "avframe_utils.h"
#include "avcodec_utils.h"
#include "avpacket_utils.h"

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/mem.h>

#include "ffmpeg-versions.h"

#define LOG0(fmt) fprintf(stderr, "[decoder:avcodec] " fmt "\n")
#define LOG1(fmt,a) fprintf(stderr, "[decoder:avcodec] " fmt "\n", (a))
#define LOG2(fmt,a,b) fprintf(stderr, "[decoder:avcodec] " fmt "\n", (a), (b))
#define LOG4(fmt,a,b,c,d) fprintf(stderr, "[decoder:avcodec] " fmt "\n", (a), (b), (c), (d))

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

#if !ICH_AVCODEC_CODECPARAMETERS
/* AVCodecParameters don't exist, we'll emulate using AVCodecContext */
typedef AVCodecContext AVCodecParameters;

static AVCodecParameters* avcodec_parameters_alloc(void) {
    return avcodec_alloc_context3(NULL);
}
static void avcodec_parameters_free(AVCodecParameters** p) {
    avcodec_free_context(p);
}

static int avcodec_parameters_copy(AVCodecParameters* dest, const AVCodecParameters* src) {
    return avcodec_copy_context(dest,src);
}

static int avcodec_parameters_to_context(AVCodecContext* dest, const AVCodecParameters* src) {
    return avcodec_copy_context(dest,src);
}
#endif

static int packet_source_to_context(AVCodecContext* dest, const packet_source* src) {
    if(src->dsi.len > 0) {
        dest->extradata = av_mallocz(src->dsi.len + AV_INPUT_BUFFER_PADDING_SIZE);
        if(dest->extradata == NULL) return -1;
        dest->extradata_size = src->dsi.len;
        memcpy(&dest->extradata[0], &src->dsi.x[0], src->dsi.len);
    }

    if(src->profile >  0) dest->profile  = src->profile - 1;
    dest->initial_padding = src->padding;
    dest->pkt_timebase.num = 1;
    dest->pkt_timebase.den = src->sample_rate;
    dest->sample_rate = src->sample_rate;
    dest->frame_size = src->frame_len;
    dest->seek_preroll = src->roll_distance;

#if ICH_AVCODEC_CTX_HAS_CH_LAYOUT
    av_channel_layout_from_mask(&dest->ch_layout, src->channel_layout);
#else
    dest->channel_layout = src->channel_layout;
#endif
    return 0;
}

static int get_ctx_channels(const AVCodecContext *ctx) {
#if ICH_AVUTIL_CHANNEL_LAYOUT
    return ctx->ch_layout.nb_channels;
#else
    return av_get_channel_layout_nb_channels(ctx->channel_layout);
#endif
}

static enum AVSampleFormat get_ctx_sample_fmt(const AVCodecContext *ctx) {
    return ctx->sample_fmt;
}

static int64_t get_ctx_mask(const AVCodecContext *ctx) {
#if ICH_AVUTIL_CHANNEL_LAYOUT
    return ctx->ch_layout.u.mask;
#else
    return ctx->channel_layout;
#endif
}

static int get_ctx_sample_rate(const AVCodecContext *ctx) {
    return ctx->sample_rate;
}

#if !(ICH_AVCODEC_SENDPACKET)
/* we have the audio decode_audio4 API, we'll create a struct to mimic send packet / get frame */
typedef struct ICH_AVCodecContext {
    AVCodecContext *codec_ctx;
    AVFrame *frame;
    AVPacket *packet;
    int flag;
} ICH_AVCodecContext;

static void ich_avcodec_free_context(ICH_AVCodecContext **ctx) {
    if( (*ctx)->codec_ctx != NULL ) {
        avcodec_free_context(& ((*ctx)->codec_ctx) );
    }

    if( (*ctx)->frame != NULL) {
        av_frame_free(& ((*ctx)->frame) );
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

    ctx->frame = av_frame_alloc();
    if(ctx->frame == NULL) {
        ich_avcodec_free_context(&ctx);
        return NULL;
    }

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

static int ich_avcodec_parameters_to_context(ICH_AVCodecContext *ctx, const AVCodecParameters *par) {
    return avcodec_parameters_to_context(ctx->codec_ctx, par);
}

static int ich_packet_source_to_context(ICH_AVCodecContext *ctx, const packet_source *src) {
    return packet_source_to_context(ctx->codec_ctx, src);
}

static int ich_avcodec_open2(ICH_AVCodecContext *ctx, const AVCodec *codec, AVDictionary **options) {
    return avcodec_open2(ctx->codec_ctx, codec, options);
}

static int ich_get_ctx_channels(const ICH_AVCodecContext *ctx) {
    return get_ctx_channels(ctx->codec_ctx);
}

static int ich_get_ctx_sample_rate(const ICH_AVCodecContext *ctx) {
    return get_ctx_sample_rate(ctx->codec_ctx);
}

static int64_t ich_get_ctx_mask(const ICH_AVCodecContext *ctx) {
    return get_ctx_mask(ctx->codec_ctx);
}

static enum AVSampleFormat ich_get_ctx_sample_fmt(const ICH_AVCodecContext *ctx) {
    return get_ctx_sample_fmt(ctx->codec_ctx);
}

static int ich_avcodec_send_packet(ICH_AVCodecContext *ctx, const AVPacket *pkt) {
    int r;
    int got;

    if(pkt == NULL) {
        pkt = ctx->packet;
    }

    if( (r = avcodec_decode_audio4(ctx->codec_ctx, ctx->frame, &got, pkt)) < 0) {
        return r;
    }

    ctx->flag = 0;
    if(got) {
        ctx->flag = 1; /* we have a regular frame to return */
    }
    if(pkt == ctx->packet) {
        ctx->flag += 2; /* entering flushing state */
    }
    /* breakdown:
     * 0 = return averror(eagain)
     * 1 = return the frame
     * 2 = return averror_eof
     * 3 = return the frame and call avcodec_decode_audio4 again
     */

    return 0;
}

static int ich_avcodec_receive_frame(ICH_AVCodecContext *ctx, AVFrame *frame) {
    switch(ctx->flag) {
        case 0: return AVERROR(EAGAIN);
        case 1: {
            av_frame_move_ref(frame,ctx->frame);
            ctx->flag = 0;
            return 0;
        }
        case 2: return AVERROR_EOF;
        default: break;
    }
    av_frame_move_ref(frame,ctx->frame);
    return ich_avcodec_send_packet(ctx, NULL);
}

#define AVCodecContext ICH_AVCodecContext
#define avcodec_send_packet ich_avcodec_send_packet
#define avcodec_receive_frame ich_avcodec_receive_frame

#define avcodec_free_context ich_avcodec_free_context
#define avcodec_alloc_context3 ich_avcodec_alloc_context3
#define avcodec_parameters_to_context ich_avcodec_parameters_to_context
#define packet_source_to_context ich_packet_source_to_context
#define avcodec_open2 ich_avcodec_open2
#define get_ctx_channels ich_get_ctx_channels
#define get_ctx_sample_rate ich_get_ctx_sample_rate
#define get_ctx_mask ich_get_ctx_mask
#define get_ctx_sample_fmt ich_get_ctx_sample_fmt
#endif

static STRBUF_CONST(plugin_name,"avcodec");

struct decoder_plugin_avcodec_userdata {
    AVCodecContext *codec_ctx;
    AVPacket *packet;
    AVFrame *avframe;
    AVCodecParameters *codec_par;
    const AVCodec* codec;
    frame frame;
};

typedef struct decoder_plugin_avcodec_userdata decoder_plugin_avcodec_userdata;

static int decoder_plugin_avcodec_init(void) {
#if ICH_AVCODEC_REGISTER_ALL
    avcodec_register_all();
#endif
    return 0;
}

static void decoder_plugin_avcodec_deinit(void) {
    return;
}

static size_t decoder_plugin_avcodec_size(void) {
    return sizeof(decoder_plugin_avcodec_userdata);
}

static int decoder_plugin_avcodec_create(void* ud) {
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)ud;

    userdata->codec     = NULL;
    userdata->codec_ctx = NULL;
    userdata->packet    = NULL;
    userdata->avframe   = NULL;
    userdata->codec_par = NULL;

    frame_init(&userdata->frame);

    return 0;
}

static int decoder_plugin_avcodec_reset(void* ud) {
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)ud;

    if(userdata->codec_ctx != NULL) avcodec_free_context(&userdata->codec_ctx);
    if(userdata->codec_par != NULL) avcodec_parameters_free(&userdata->codec_par);
    if(userdata->avframe != NULL) av_frame_free(&userdata->avframe);
    if(userdata->packet != NULL) {
        av_packet_free(&userdata->packet);
    }

    frame_free(&userdata->frame);
    return 0;
}

static void decoder_plugin_avcodec_close(void* ud) {
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)ud;
    decoder_plugin_avcodec_reset(userdata);
}

static int decoder_plugin_avcodec_open(void* ud, const packet_source* src, const frame_receiver* dest) {
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)ud;
    frame_source me = FRAME_SOURCE_ZERO;

    char av_errbuf[128];
    int av_err;
    enum AVCodecID av_id;

    if(strbuf_equals_cstr(src->name,"avformat")) {
        /* if the source is the avformat plugin we'll load private data from that */
        if(userdata->codec_par != NULL) {
            avcodec_parameters_free(&userdata->codec_par);
        }
        userdata->codec_par = avcodec_parameters_alloc();
        if(userdata->codec_par == NULL) {
            return -1;
        }
        avcodec_parameters_copy(userdata->codec_par, src->priv);
        av_id = userdata->codec_par->codec_id;
    } else {
        av_id = codec_to_avcodec(src->codec);
    }

    if(av_id == AV_CODEC_ID_NONE) {
        LOG0("failed to find appropriate codec id");
        return -1;
    }

    userdata->codec = avcodec_find_decoder(av_id);
    if(userdata->codec == NULL) {
        LOG0("failed to find decoder for codec");
        return -1;
    }

    userdata->codec_ctx = avcodec_alloc_context3(userdata->codec);
    if(userdata->codec_ctx == NULL) {
        LOG0("error allocating AVCodecContext");
        return -1;
    }

    if(userdata->codec_par != NULL) {
        avcodec_parameters_to_context(userdata->codec_ctx,userdata->codec_par);
    } else {
        packet_source_to_context(userdata->codec_ctx,src);
    }

    if( (av_err = avcodec_open2(userdata->codec_ctx, userdata->codec, NULL)) < 0) {
        av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
        LOG1("error with avcodec_open2: %s", av_errbuf);
        return -1;
    }

    userdata->avframe = av_frame_alloc();
    if(userdata->avframe == NULL) {
        LOG0("failed to allocate avframe");
        return -1;
    }

    userdata->packet = av_packet_alloc();
    if(userdata->packet == NULL) {
        LOG0("failed to allocate avpacket");
        return -1;
    }

    userdata->frame.channels = get_ctx_channels(userdata->codec_ctx);
    userdata->frame.format = avsampleformat_to_samplefmt(get_ctx_sample_fmt(userdata->codec_ctx));

    if(userdata->frame.format == SAMPLEFMT_UNKNOWN) {
        LOG0("unknown sample format");
        return -1;
    }

    if(frame_ready(&userdata->frame) != 0) {
        LOG0("error allocating output frame");
        return -1;
    }

    me.channel_layout = get_ctx_mask(userdata->codec_ctx);
    me.sample_rate = get_ctx_sample_rate(userdata->codec_ctx);
    me.format = userdata->frame.format;

    return dest->open(dest->handle, &me);
}

static int decoder_plugin_avcodec_config(void* ud, const strbuf* key, const strbuf* value) {
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)ud;
    (void)userdata;
    (void)key;
    (void)value;

    return 0;
}

static int decoder_plugin_avcodec_decode(void* ud, const packet* p, const frame_receiver* frame_dest) {
    int r;
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)ud;
    int av_err;
    char av_errbuf[128];

    if( (r = packet_to_avpacket(userdata->packet, p)) != 0) {
        LOG0("error converting packet to avpacket");
        return -1;
    }

    if( (av_err = avcodec_send_packet(userdata->codec_ctx, userdata->packet)) != 0) {
        av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
        LOG1("error with avcodec_send_packet: %s", av_errbuf);
        return -1;
    }
    av_packet_unref(userdata->packet);

    av_err = avcodec_receive_frame(userdata->codec_ctx, userdata->avframe);
    if(av_err != 0) {
        if(av_err == AVERROR(EAGAIN)) return 0;
        av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
        LOG1("error with avcodec_receive_frame: %s", av_errbuf);
        return -1;
    }

    av_err = avframe_to_frame(&userdata->frame, userdata->avframe);
    av_frame_unref(userdata->avframe);
    if(av_err != 0) {
        LOG0("error converting avframe to frame");
        return -1;
    }
    return frame_dest->submit_frame(frame_dest->handle,&userdata->frame);
}

static int decoder_plugin_avcodec_flush(void* ud, const frame_receiver* dest) {
    int r;
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)ud;
    int av_err;
    char av_errbuf[128];

    if( (av_err = avcodec_send_packet(userdata->codec_ctx, NULL)) != 0) {
        av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
        LOG1("error with avcodec_send_packet: %s", av_errbuf);
        return -1;
    }

    av_err = avcodec_receive_frame(userdata->codec_ctx, userdata->avframe);
    if(av_err != 0) {
        if(av_err == AVERROR(EAGAIN)) goto destflush;
        if(av_err == AVERROR_EOF) goto destflush;
        av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
        LOG1("error with avcodec_receive_frame: %s", av_errbuf);
        return -1;
    }

    av_err = avframe_to_frame(&userdata->frame, userdata->avframe);
    av_frame_unref(userdata->avframe);
    if(av_err != 0) {
        LOG0("error converting avframe to frame");
        return -1;
    }
    if( (r = dest->submit_frame(dest->handle,&userdata->frame)) != 0) return r;

    destflush:
    return 0;
}

const decoder_plugin decoder_plugin_avcodec = {
    plugin_name,
    decoder_plugin_avcodec_size,
    decoder_plugin_avcodec_init,
    decoder_plugin_avcodec_deinit,
    decoder_plugin_avcodec_create,
    decoder_plugin_avcodec_config,
    decoder_plugin_avcodec_open,
    decoder_plugin_avcodec_close,
    decoder_plugin_avcodec_decode,
    decoder_plugin_avcodec_flush,
    decoder_plugin_avcodec_reset,
};
