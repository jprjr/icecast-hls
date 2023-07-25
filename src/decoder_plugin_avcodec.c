#include "decoder_plugin_avcodec.h"

#include "avframe_utils.h"
#include "avcodec_utils.h"
#include "avpacket_utils.h"

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/mem.h>

#define LOG0(fmt) fprintf(stderr, "[decoder:avcodec] " fmt "\n")
#define LOG1(fmt,a) fprintf(stderr, "[decoder:avcodec] " fmt "\n", (a))
#define LOG2(fmt,a,b) fprintf(stderr, "[decoder:avcodec] " fmt "\n", (a), (b))
#define LOG4(fmt,a,b,c,d) fprintf(stderr, "[decoder:avcodec] " fmt "\n", (a), (b), (c), (d))

static STRBUF_CONST(plugin_name,"avcodec");

struct decoder_plugin_avcodec_userdata {
    AVCodecContext *codec_ctx;
    AVPacket *packet;
    AVFrame *avframe;
#if LIBAVCODEC_VERSION_MAJOR >= 59
    const AVCodec* codec;
#else
    AVCodec* codec;
#endif
    frame frame;
};

typedef struct decoder_plugin_avcodec_userdata decoder_plugin_avcodec_userdata;

static int decoder_plugin_avcodec_init(void) {
#if LIBAVCODEC_VERSION_MAJOR < 58
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

    frame_init(&userdata->frame);

    return 0;
}

static void decoder_plugin_avcodec_close(void* ud) {
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)ud;

    if(userdata->codec_ctx != NULL) avcodec_free_context(&userdata->codec_ctx);
    if(userdata->avframe != NULL) av_frame_free(&userdata->avframe);
    if(userdata->packet != NULL) {
#if LIBAVCODEC_VERSION_MAJOR < 57
        av_free(userdata->packet);
        userdata->packet = NULL;
#else
        av_packet_free(&userdata->packet);
#endif
    }

    frame_free(&userdata->frame);
}

static int decoder_plugin_avcodec_open(void* ud, const packet_source* src, const frame_receiver* dest) {
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)ud;
    frame_source me = FRAME_SOURCE_ZERO;

    char av_errbuf[128];
    int av_err;
    enum AVCodecID av_id;

    av_id = codec_to_avcodec(src->codec);

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

    if(src->dsi.len > 0) {
        userdata->codec_ctx->extradata = av_mallocz(src->dsi.len + AV_INPUT_BUFFER_PADDING_SIZE);
        userdata->codec_ctx->extradata_size = src->dsi.len;
        memcpy(&userdata->codec_ctx->extradata[0], &src->dsi.x[0], src->dsi.len);
    }

    userdata->codec_ctx->codec_tag   = src->codec_tag;
    userdata->codec_ctx->block_align = src->block_align;
    userdata->codec_ctx->bit_rate   = src->bit_rate;
    userdata->codec_ctx->bits_per_coded_sample   = src->bits_per_coded_sample;
    userdata->codec_ctx->bits_per_raw_sample   = src->bits_per_raw_sample;
    userdata->codec_ctx->profile   = src->avprofile;
    userdata->codec_ctx->level   = src->avlevel;
    userdata->codec_ctx->trailing_padding   = src->trailing_padding;
    userdata->codec_ctx->initial_padding = src->padding;
    userdata->codec_ctx->pkt_timebase.num = 1;
    userdata->codec_ctx->pkt_timebase.den = src->sample_rate;
    userdata->codec_ctx->sample_rate = src->sample_rate;
    userdata->codec_ctx->frame_size = src->frame_len;
    userdata->codec_ctx->seek_preroll = src->roll_distance;

#if LIBAVCODEC_VERSION_MAJOR >= 60
    av_channel_layout_from_mask(&userdata->codec_ctx->ch_layout, src->channel_layout);
#else
    userdata->codec_ctx->channel_layout = src->channel_layout;
#endif

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

#if LIBAVCODEC_VERSION_MAJOR >= 57
    userdata->packet = av_packet_alloc();
#else
    userdata->packet = av_mallocz(sizeof(AVPacket));
#endif
    if(userdata->packet == NULL) {
        LOG0("failed to allocate avpacket");
        return -1;
    }

#if LIBAVCODEC_VERSION_MAJOR < 57
    av_init_packet(userdata->packet);
#endif

#if LIBAVCODEC_VERSION_MAJOR >= 60
    userdata->frame.channels = userdata->codec_ctx->ch_layout.nb_channels;
#else
    userdata->frame.channels = av_get_channel_layout_nb_channels(userdata->codec_ctx->channel_layout);
#endif
    userdata->frame.format = avsampleformat_to_samplefmt(userdata->codec_ctx->sample_fmt);
    if(userdata->frame.format == SAMPLEFMT_UNKNOWN) {
        LOG0("unknown sample format");
        return -1;
    }

    if(frame_ready(&userdata->frame) != 0) {
        LOG0("error allocating output frame");
        return -1;
    }

#if LIBAVCODEC_VERSION_MAJOR >= 60
    me.channel_layout = userdata->codec_ctx->ch_layout.u.mask;
#else
    me.channel_layout = userdata->codec_ctx->channel_layout;
#endif
    me.format = userdata->frame.format;
    me.sample_rate = userdata->codec_ctx->sample_rate;

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
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57,48,0) /* ffmpeg-3.1 */
    int got;
#endif

    if( (r = packet_to_avpacket(userdata->packet, p)) != 0) {
        LOG0("error converting packet to avpacket");
        return -1;
    }

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,0) /* ffmpeg-3.1 */
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
#else
    if( (av_err = avcodec_decode_audio4(userdata->codec_ctx, frame, &got, userdata->packet)) < 0) {
        av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
        LOG1("error with avcodec_decode_audio4: %s", av_errbuf);
        return -1;
    }
    av_err = 0;
    av_packet_unref(userdata->packet);
    if(got) {
#endif
    av_err = avframe_to_frame(&userdata->frame, userdata->avframe);
    av_frame_unref(userdata->avframe);
    if(av_err != 0) {
        LOG0("error converting avframe to frame");
        return -1;
    }
    return frame_dest->submit_frame(frame_dest->handle,&userdata->frame);
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,0) /* ffmpeg-3.1 */
#else
    }
    return 0;
#endif
}

static int decoder_plugin_avcodec_flush(void* ud, const frame_receiver* dest) {
    int r;
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)ud;
    int av_err;
    char av_errbuf[128];
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57,48,0) /* ffmpeg-3.1 */
    int got;
    packet p;
    av_packet_init(&p);
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,0) /* ffmpeg-3.1 */
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
#else
    p.data = NULL;
    p.size = 0;
    if( (av_err = avcodec_decode_audio4(userdata->codec_ctx, frame, &got, &p)) < 0) {
        if(av_err == AVERROR(EAGAIN)) goto destflush;
        if(av_err == AVERROR_EOF) goto destflush;
        av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
        LOG1("error with avcodec_decode_audio4: %s", av_errbuf);
        return -1;
    }
    av_err = 0;
    if(got) {
#endif
    av_err = avframe_to_frame(&userdata->frame, userdata->avframe);
    av_frame_unref(userdata->avframe);
    if(av_err != 0) {
        LOG0("error converting avframe to frame");
        return -1;
    }
    if( (r = dest->submit_frame(dest->handle,&userdata->frame)) != 0) return r;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,0) /* ffmpeg-3.1 */
#else
    }
#endif

    destflush:
    return 0;
}

static int decoder_plugin_avcodec_reset(void* ud) {
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)ud;

    if(userdata->codec_ctx != NULL) avcodec_free_context(&userdata->codec_ctx);
    if(userdata->avframe != NULL) av_frame_free(&userdata->avframe);
    if(userdata->packet != NULL) {
#if LIBAVCODEC_VERSION_MAJOR < 57
        av_free(userdata->packet);
        userdata->packet = NULL;
#else
        av_packet_free(&userdata->packet);
#endif
    }

    frame_free(&userdata->frame);
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
