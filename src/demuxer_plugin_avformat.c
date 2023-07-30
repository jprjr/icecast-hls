#include "demuxer_plugin_avformat.h"

#include "avpacket_utils.h"
#include "avcodec_utils.h"

#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavcodec/avcodec.h>

#include "ffmpeg-versions.h"
#if ICH_AVCODEC_BSFH
#include <libavcodec/bsf.h>
#endif

#include <stdio.h>

#define LOG0(fmt) fprintf(stderr, "[demuxer:avformat] " fmt "\n")
#define LOG1(fmt,a) fprintf(stderr, "[demuxer:avformat] " fmt "\n", (a))
#define LOG2(fmt,a,b) fprintf(stderr, "[demuxer:avformat] " fmt "\n", (a), (b))
#define LOG4(fmt,a,b,c,d) fprintf(stderr, "[demuxer:avformat] " fmt "\n", (a), (b), (c), (d))

#define BUFFER_SIZE 4096

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
#endif


#if !ICH_AVCODEC_BSF_INIT
/* we have the old bitstream filter API :-/ */

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

typedef struct AVBSFContext {
    /* basically save the arguments needed for av_bitstream_filter_filter */
    AVBitStreamFilterContext *bsfc;
    const char *args;
    AVPacket *packet; /* covers poutbuf and poutbuf_size */
    /* buf, bufsize, and keyframe will be provided by input packet */
    uint8_t flag; /* 0 = return AVERROR(EAGAIN), 1 = packet, 2 = eof */

    AVCodecParameters *par_in;
    AVCodecParameters *par_out;
    AVRational time_base_in;
    AVRational time_base_out;
} AVBSFContext;

/* provide a av_bsf_get_by_name function */
static const AVBitStreamFilter *av_bsf_get_by_name(const char *name) {
    const AVBitStreamFilter *filter = NULL;
    while( (filter = av_bitstream_filter_next(filter)) != NULL) {
        if(strcmp(filter->name,name) == 0) break;
    }
    return filter;
}

static void av_bsf_free(AVBSFContext **ctx) {
    if( (*ctx) != NULL) {
        if( (*ctx)->bsfc != NULL) {
            av_bitstream_filter_close( (*ctx)->bsfc );
            (*ctx)->bsfc = NULL;
        }
        if( (*ctx)->packet != NULL) {
            av_packet_free(&( (*ctx)->packet ));
        }
        if( (*ctx)->par_in != NULL) {
            avcodec_parameters_free(&(*ctx)->par_in);
        }
        if( (*ctx)->par_out != NULL) {
            avcodec_parameters_free(&(*ctx)->par_out);
        }
        av_free(*ctx);
        *ctx = NULL;
    }
}

static int av_bsf_alloc(const AVBitStreamFilter *filter, AVBSFContext **ctx) {
    (*ctx) = av_mallocz(sizeof(AVBSFContext));
    if( (*ctx) == NULL) return -1;

    (*ctx)->par_in = NULL;
    (*ctx)->par_out = NULL;
    (*ctx)->args = NULL;
    (*ctx)->packet = NULL;

    (*ctx)->bsfc = av_bitstream_filter_init(filter->name);
    if( (*ctx)->bsfc == NULL) {
        av_bsf_free(ctx);
        return -1;
    }

    (*ctx)->packet = av_packet_alloc();
    if( (*ctx)->packet == NULL) {
        av_bsf_free(ctx);
        return -1;
    }

    (*ctx)->par_in = avcodec_parameters_alloc();
    if( (*ctx)->par_in == NULL) {
        av_bsf_free(ctx);
        return -1;
    }

    (*ctx)->par_out = avcodec_parameters_alloc();
    if( (*ctx)->par_out == NULL) {
        av_bsf_free(ctx);
        return -1;
    }

    return 0;
}

static int av_bsf_init(AVBSFContext *ctx) {
    ctx->time_base_out = ctx->time_base_in; /* fingers
    crossed none of the filters modify the time base? */
    avcodec_parameters_copy(ctx->par_out,ctx->par_in);
    return ctx->bsfc == NULL;
}

static int av_bsf_send_packet(AVBSFContext *ctx, AVPacket *pkt) {
    int r;
    if(pkt == NULL) {
        ctx->flag = 2;
        return 0;
    }

    av_packet_unref(ctx->packet);
    if( (r = av_bitstream_filter_filter(
          ctx->bsfc,
          ctx->par_in,
          ctx->args,
          &ctx->packet->data,
          &ctx->packet->size,
          pkt->data,
          pkt->size,
          pkt->flags & AV_PKT_FLAG_KEY)) < 0) {
        return r;
    }
    if( (r = av_packet_make_writable(ctx->packet)) != 0) return r;

    /* since we're pretty sure none of these mess with
     * the time base we can just copy timing info as-is */
    ctx->packet->pts = pkt->pts;
    ctx->packet->dts = pkt->dts;
    ctx->packet->stream_index = pkt->stream_index;
    ctx->packet->flags = pkt->flags;
    ctx->packet->duration = pkt->duration;
    ctx->packet->pos = pkt->pos;
#if ICH_AVCODEC_CONVERGENCE && FF_API_CONVERGENCE_DURATION
    ctx->packet->convergence_duration = pkt->convergence_duration;
#endif
    if( (r= av_copy_packet_side_data(ctx->packet,pkt)) != 0) return r;

    ctx->flag = 1;
    av_packet_unref(pkt);
    return 0;
}

static int av_bsf_receive_packet(AVBSFContext *ctx, AVPacket *pkt) {
    switch(ctx->flag) {
        case 2: return AVERROR_EOF;
        case 0: return AVERROR(EAGAIN);
        default: break;
    }

    av_packet_unref(pkt);
    av_packet_move_ref(pkt,ctx->packet);

    ctx->flag = 0;
    return 0;
}

#endif

#if (!ICH_AVCODEC_BSF_FLUSH)
static void av_bsf_flush(AVBSFContext *ctx) {
    av_bsf_send_packet(ctx, NULL);
}
#endif

#if (!ICH_AVCODEC_GET_NULL_FILTER)
/* provide wrappers around av_bsf_* functions to create a fake "null" filter */
typedef struct ICH_AVBSFContext {
    /* the actual conext, set to NULL for our "null" filter */
    AVBSFContext *ctx;

    /* our buffered packet */
    AVPacket *pkt;
    int flag;

    /* codec parameters for init, ignored on older ffmpeg */
    AVCodecParameters *par_in;
    AVCodecParameters *par_out;
    AVRational time_base_in;
    AVRational time_base_out;
} ICH_AVBSFContext;

static void ich_av_bsf_free(ICH_AVBSFContext **ctx) {
    if(!ctx || !*ctx) return;

    if( (*ctx)->ctx != NULL) {
        av_bsf_free(& (*ctx)->ctx);
    }

    if( (*ctx)->pkt != NULL) {
        av_packet_free( &( (*ctx)->pkt) );
    }

    avcodec_parameters_free(&( (*ctx)->par_in) );
    avcodec_parameters_free(&( (*ctx)->par_out) );

    av_free(*ctx);
    *ctx = NULL;
}

static int ich_av_bsf_alloc(const AVBitStreamFilter *filter, ICH_AVBSFContext **ctx) {
    int r;
    ICH_AVBSFContext *c = av_mallocz(sizeof(ICH_AVBSFContext));
    if(c == NULL) return AVERROR(ENOMEM);

    c->ctx = NULL;
    c->pkt = NULL;

    c->par_in = avcodec_parameters_alloc();
    if(c->par_in == NULL) {
        r = AVERROR(ENOMEM);
        goto cleanup;
    }

    c->par_out = avcodec_parameters_alloc();
    if(c->par_out == NULL) {
        r = AVERROR(ENOMEM);
        goto cleanup;
    }

    if(filter == NULL) {
        c->pkt = av_packet_alloc();
        if(c->pkt == NULL) {
            r = AVERROR(ENOMEM);
            goto cleanup;
        }
    }
    else {
        r = av_bsf_alloc(filter, &c->ctx);
        if(r != 0) goto cleanup;
    }

    *ctx = c;
    return 0;

    cleanup:
    ich_av_bsf_free(ctx);
    return r;
}

static int ich_av_bsf_get_null_filter(ICH_AVBSFContext **ctx) {
    return ich_av_bsf_alloc(NULL, ctx);
}

static int ich_av_bsf_init(ICH_AVBSFContext *ctx) {
    int r;
    if(ctx->ctx != NULL) {
        avcodec_parameters_copy(ctx->ctx->par_in,ctx->par_in);
        ctx->ctx->time_base_in = ctx->time_base_in;
        if( (r = av_bsf_init(ctx->ctx)) != 0) return r;
        avcodec_parameters_copy(ctx->par_out,ctx->ctx->par_out);
        ctx->time_base_out = ctx->ctx->time_base_out;
        return 0;
    }

    avcodec_parameters_copy(ctx->par_out,ctx->par_in);
    ctx->time_base_out = ctx->time_base_in;
    return 0;
}

static int ich_av_bsf_send_packet(ICH_AVBSFContext *ctx, AVPacket *pkt) {
    if(ctx->ctx != NULL) return av_bsf_send_packet(ctx->ctx, pkt);

    av_packet_unref(ctx->pkt);
    av_packet_move_ref(ctx->pkt,pkt);

    ctx->flag = 1;
    return 0;
}

static void ich_av_bsf_flush(ICH_AVBSFContext *ctx) {
    if(ctx->ctx != NULL) {
        av_bsf_flush(ctx->ctx);
        return;
    }
    ctx->flag = 2;
}

static int ich_av_bsf_receive_packet(ICH_AVBSFContext *ctx, AVPacket *pkt) {
    if(ctx->ctx != NULL) return av_bsf_receive_packet(ctx->ctx, pkt);

    switch(ctx->flag) {
        case 0: return AVERROR(EAGAIN);
        case 2: return AVERROR_EOF;
        default: break;
    }

    av_packet_unref(pkt);
    av_packet_move_ref(pkt,ctx->pkt);

    ctx->flag = 0;
    return 0;
}

#define AVBSFContext ICH_AVBSFContext
#define av_bsf_free ich_av_bsf_free
#define av_bsf_alloc ich_av_bsf_alloc
#define av_bsf_init ich_av_bsf_init
#define av_bsf_send_packet ich_av_bsf_send_packet
#define av_bsf_receive_packet ich_av_bsf_receive_packet
#define av_bsf_flush ich_av_bsf_flush
#define av_bsf_get_null_filter ich_av_bsf_get_null_filter

#endif

static STRBUF_CONST(plugin_name, "avformat");

struct demuxer_plugin_avformat_userdata {
    uint8_t* buffer;
    AVIOContext* io_ctx;
    AVFormatContext* fmt_ctx;
    AVPacket *av_packet;
    AVBSFContext *bsf;
    AVRational tb_src;
    AVRational tb_dest;
#if ICH_AVFORMAT_FIND_BEST_STREAM_CONST
    const AVCodec* codec;
#else
    AVCodec* codec;
#endif
    int audioStreamIndex;
    packet packet;
    strbuf bsf_filters;
    packet_source me;
};

typedef struct demuxer_plugin_avformat_userdata demuxer_plugin_avformat_userdata;

static void* priv_data_dupe(void *priv) {
    AVCodecParameters *dest = NULL;
    AVCodecParameters *src  = NULL;
    src = (AVCodecParameters *)priv;

    dest = avcodec_parameters_alloc();
    if(dest == NULL) return dest;
    avcodec_parameters_copy(dest,src);
    return dest;
}

static void priv_data_free(void *priv) {
    AVCodecParameters *codecpar = (AVCodecParameters *)priv;
    avcodec_parameters_free(&codecpar);
}

static int load_packet_source_codecpar(demuxer_plugin_avformat_userdata* userdata, const AVCodecParameters* codecpar) {
    userdata->me.sync_flag = 1; /* TODO check if there's any codecs that are non-sync */

    userdata->tb_dest.num = 1;
    userdata->tb_dest.den = codecpar->sample_rate;

    userdata->me.sample_rate = codecpar->sample_rate;
    userdata->me.frame_len = codecpar->frame_size;
    userdata->me.padding = codecpar->initial_padding;
    userdata->me.roll_distance = codecpar->seek_preroll;

    if(userdata->me.priv != NULL) {
        avcodec_parameters_free((AVCodecParameters **) &userdata->me.priv);
    }
    userdata->me.priv = avcodec_parameters_alloc();
    if(userdata->me.priv == NULL) return -1;
    avcodec_parameters_copy(userdata->me.priv, codecpar);
    userdata->me.priv_copy = priv_data_dupe;
    userdata->me.priv_free = priv_data_free;


#if ICH_AVUTIL_CHANNEL_LAYOUT
    switch(codecpar->ch_layout.order) {
        case AV_CHANNEL_ORDER_NATIVE: userdata->me.channel_layout = codecpar->ch_layout.u.mask; break;
        case AV_CHANNEL_ORDER_UNSPEC: {
            switch(codecpar->ch_layout.nb_channels) {
                case 1: {
                    LOG0("warning, guessing mono for channel layout");
                    userdata->me.channel_layout = LAYOUT_MONO;
                    break;
                }
                case 2: {
                    LOG0("warning, guessing stereo for channel layout");
                    userdata->me.channel_layout = LAYOUT_STEREO;
                    break;
                }
                default: {
                    LOG1("unspecified channel layout, channels=%d",codecpar->ch_layout.nb_channels);
                    return -1;
                }
            }
            /* need to set the channel layout on the codec parameters, too */
            av_channel_layout_from_mask( &((AVCodecParameters *)userdata->me.priv)->ch_layout , userdata->me.channel_layout);
            break;
        }
        /* fall-through */
        default: {
            LOG0("unknown channel layout");
            return -1;
        }
    }
#else
    userdata->me.channel_layout = codecpar->channel_layout;
#endif

    switch(userdata->codec->id) {
        case AV_CODEC_ID_NONE: {
            LOG0("unknown codec");
            return -1;
        }
        case AV_CODEC_ID_AAC: {
            if(codecpar->profile < 0) {
                /* TODO seekable input for files */
                LOG0("Unable to detect AAC profile, is this file streamable?");
                return -1;
            }
            userdata->me.profile = codecpar->profile + 1;
            switch(userdata->me.profile) {
                case CODEC_PROFILE_AAC_USAC: userdata->me.sync_flag = 0; /* fall-through */
                case CODEC_PROFILE_AAC_LC: /* fall-through */
                case CODEC_PROFILE_AAC_HE: /* fall-through */
                case CODEC_PROFILE_AAC_HE2: /* fall-through */
                case CODEC_PROFILE_AAC_LAYER3: break;
                default: {
                    LOG1("Unsupported AAC profile %u", userdata->me.profile);
                    return -1;
                }
            }

            break;
        }
        default: {
            break;
        }
    }

    if(codecpar->extradata_size) {
        return membuf_append(&userdata->me.dsi,codecpar->extradata,codecpar->extradata_size);
    }

    return 0;
}

static int load_packet_source(demuxer_plugin_avformat_userdata* userdata, const AVStream* stream) {

    userdata->tb_src = stream->time_base;
    userdata->me.codec = avcodec_to_codec(userdata->codec->id);
    if(userdata->me.codec == CODEC_TYPE_UNKNOWN) {
        LOG1("Unknown avcodec id: %u", userdata->codec->id);
        return -1;
    }

    return load_packet_source_codecpar(userdata,
#if ICH_AVFORMAT_STREAM_CODECPAR
      stream->codecpar
#else
      stream->codec
#endif
    );

}

static int demuxer_plugin_avformat_init(void) {
#if ICH_AVFORMAT_REGISTER_ALL
    av_register_all();
#endif
    return 0;
}

static void demuxer_plugin_avformat_deinit(void) {
    return;
}

static size_t demuxer_plugin_avformat_size(void) {
    return sizeof(demuxer_plugin_avformat_userdata);
}

static int demuxer_plugin_avformat_config(void* ud, const strbuf* key, const strbuf* val) {
    demuxer_plugin_avformat_userdata* userdata = (demuxer_plugin_avformat_userdata*)ud;
    int r;

    if(strbuf_equals_cstr(key,"bsf filters") ||
       strbuf_equals_cstr(key,"bsf-filters") ||
       strbuf_equals_cstr(key,"bitstream-filters") ||
       strbuf_equals_cstr(key,"bitstream- ilters")) {
        if(userdata->bsf_filters.len > 0) {
            LOG0("only 1 filter string is supported");
            return -1;
        }
        if( (r = strbuf_copy(&userdata->bsf_filters,val)) != 0) {
            LOG0("out of memory");
            return r;
        }
        if( (r = strbuf_term(&userdata->bsf_filters)) != 0) {
            LOG0("out of memory");
            return r;
        }
        return 0;
    }

    LOG2("unknown config key %.*s",
     (int)key->len,(char *)key->x);
    return -1;
}

static int demuxer_plugin_avformat_create(void* ud) {
    demuxer_plugin_avformat_userdata* userdata = (demuxer_plugin_avformat_userdata*)ud;

    userdata->buffer     = NULL;
    userdata->io_ctx     = NULL;
    userdata->fmt_ctx    = NULL;
    userdata->codec      = NULL;
    userdata->av_packet  = NULL;
    userdata->bsf        = NULL;
    packet_init(&userdata->packet);
    strbuf_init(&userdata->bsf_filters);

    userdata->me = packet_source_zero;

    return 0;
}

static void demuxer_plugin_avformat_close(void* ud) {
    demuxer_plugin_avformat_userdata* userdata = (demuxer_plugin_avformat_userdata*)ud;

    if(userdata->fmt_ctx != NULL) avformat_close_input(&userdata->fmt_ctx);

    if(userdata->me.priv != NULL) {
        avcodec_parameters_free( (AVCodecParameters **) &userdata->me.priv);
    }

    if(userdata->io_ctx != NULL) {
        av_free(userdata->io_ctx->buffer);
        av_free(userdata->io_ctx);
        userdata->buffer = NULL;
    }
    if(userdata->buffer != NULL) av_free(userdata->io_ctx);
    if(userdata->av_packet != NULL) {
        av_packet_free(&userdata->av_packet);
    }

    if(userdata->bsf != NULL) {
        av_bsf_free(&userdata->bsf);
    }

    packet_free(&userdata->packet);
    strbuf_free(&userdata->bsf_filters);
    packet_source_free(&userdata->me);
}

static int demuxer_plugin_avformat_read(void *ud, uint8_t *buf, int buf_size) {
    input* in = (input*)ud;
    size_t r = input_read(in, buf, (size_t)buf_size);

    if(r == 0) return AVERROR_EOF;
    return (int)r;
}

static int demuxer_plugin_avformat_open(void* ud, input* in) {
    demuxer_plugin_avformat_userdata* userdata = (demuxer_plugin_avformat_userdata*)ud;
    char av_errbuf[128];
    int av_err;
#if ICH_AVFORMAT_FIND_BEST_STREAM_CONST
    const AVInputFormat *fmt = NULL;
#else
    AVInputFormat *fmt = NULL;
#endif

    userdata->av_packet = av_packet_alloc();

    userdata->buffer = av_malloc(BUFFER_SIZE + AVPROBE_PADDING_SIZE);
    if(userdata->buffer == NULL) {
        LOG0("failed to allocate buffer");
        return -1;
    }
    userdata->io_ctx = avio_alloc_context(userdata->buffer, BUFFER_SIZE,
      0, /* write flag */
      (void *)in,
      demuxer_plugin_avformat_read,
      NULL,
      NULL);

    if(userdata->io_ctx == NULL) {
        LOG0("failed to allocate io context");
        return -1;
    }

    if(av_probe_input_buffer(userdata->io_ctx, &fmt, "", NULL, 0, 1024 * 32) != 0) {
        LOG0("failed to probe input");
        return -1;
    }

    userdata->fmt_ctx = avformat_alloc_context();
    if(userdata->fmt_ctx == NULL) {
        LOG0("failed to allocate format context");
        return -1;
    }

    userdata->fmt_ctx->pb = userdata->io_ctx;
    userdata->fmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

    if( (av_err = avformat_open_input(&userdata->fmt_ctx, "", fmt, NULL)) < 0) {
        av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
        LOG1("error with avformat_open_input: %s", av_errbuf);
        return -1;
    }

    return 0;
}

static int demuxer_plugin_avformat_submit_packet(demuxer_plugin_avformat_userdata* userdata, const packet_receiver* receiver) {
    int r;
    av_packet_rescale_ts(userdata->av_packet, userdata->tb_src, userdata->tb_dest);

    if(avpacket_to_packet(&userdata->packet,userdata->av_packet) != 0) {
      LOG0("unable to convert packet");
      return -1;
    }

    /* if we rescaled the sample_rate seems to get overwritten to 1 or 0? */
    userdata->packet.sample_rate = userdata->tb_dest.den;

    if( (r = receiver->submit_packet(receiver->handle, &userdata->packet)) != 0) return r;
    av_packet_unref(userdata->av_packet);

    return 0;
}

static int demuxer_plugin_avformat_run(void* ud, const tag_handler* thandler, const packet_receiver* receiver) {
    demuxer_plugin_avformat_userdata* userdata = (demuxer_plugin_avformat_userdata*)ud;
    char av_errbuf[128];
    int av_err;
    int r;
    unsigned int i;
    AVStream *stream = NULL;
#if (!ICH_AVCODEC_AVBSFLIST)
    const AVBitStreamFilter *bsf = NULL;
#endif

    (void)thandler;

    if(userdata->codec == NULL) {
        if( (av_err = avformat_find_stream_info(userdata->fmt_ctx, NULL)) < 0) {
            av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
            LOG1("error with avformat_find_stream_info: %s", av_errbuf);
            return -1;
        }

        for(i=0;i<userdata->fmt_ctx->nb_streams;i++) {
            userdata->fmt_ctx->streams[i]->discard = AVDISCARD_ALL;
        }

        if( (userdata->audioStreamIndex = av_find_best_stream(userdata->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &userdata->codec, 0)) < 0) {
            av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
            LOG1("error with avformat_find_best_stream: %s", av_errbuf);
            return -1;
        }

        stream = userdata->fmt_ctx->streams[userdata->audioStreamIndex];
        stream->discard = AVDISCARD_DEFAULT;

        userdata->me.handle = userdata;
        userdata->me.name = &plugin_name;

        if( (r = load_packet_source(userdata,stream)) != 0) return r;

        if(userdata->bsf_filters.len > 0) {
#if ICH_AVCODEC_AVBSFLIST
            if( (av_err = av_bsf_list_parse_str((const char *)userdata->bsf_filters.x,&userdata->bsf)) < 0) {
                av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
                LOG1("error parsing bsf list: %s", av_errbuf);
                return -1;
            }
#else /* fallback for older ffmpeg */
            bsf = av_bsf_get_by_name((const char *)userdata->bsf_filters.x);
            if(bsf == NULL) {
                LOG1("unable to find bitstream filter %s",(const char *)userdata->bsf_filters.x);
                return -1;
            }
            if( (av_err = av_bsf_alloc(bsf,&userdata->bsf)) < 0) {
                av_strerror(av_err,av_errbuf,sizeof(av_errbuf));
                LOG1("error allocating bitstream filter: %s", av_errbuf);
                return -1;
            }
#endif
        } else {
            if( (av_err = av_bsf_get_null_filter(&userdata->bsf)) < 0) {
                av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
                LOG1("error getting bsf null filter: %s", av_errbuf);
                return -1;
            }
        }

#if ICH_AVFORMAT_STREAM_CODECPAR
        avcodec_parameters_copy(userdata->bsf->par_in,stream->codecpar);
#else
        avcodec_parameters_copy(userdata->bsf->par_in,stream->codec);
#endif
        userdata->bsf->time_base_in = stream->time_base;

        if( (av_err = av_bsf_init(userdata->bsf)) < 0) {
            av_strerror(av_err,av_errbuf,sizeof(av_errbuf));
            LOG1("error initializing bsf filters: %s", av_errbuf);
            return -1;
        }

        return receiver->open(receiver->handle, &userdata->me);
    }

    stream = userdata->fmt_ctx->streams[userdata->audioStreamIndex];

    if( (av_err = av_read_frame(userdata->fmt_ctx, userdata->av_packet)) == 0) {
        if(userdata->av_packet->stream_index != userdata->audioStreamIndex) return 0;
        if(( av_err = av_bsf_send_packet(userdata->bsf,userdata->av_packet)) != 0) {
            av_strerror(av_err,av_errbuf,sizeof(av_errbuf));
            LOG1("error sending packet to bsf: %s", av_errbuf);
            return -1;
        }

        while( (av_err = av_bsf_receive_packet(userdata->bsf, userdata->av_packet)) == 0) {
            if( (r = demuxer_plugin_avformat_submit_packet(userdata,receiver)) != 0) return r;
        }

        if(av_err == AVERROR(EAGAIN)) return 0;
        if(av_err == AVERROR_EOF) return 1;
        av_strerror(av_err,av_errbuf,sizeof(av_errbuf));
        LOG1("error receiving packet from bsf: %s", av_errbuf);
        return -1;
    }

    if(av_err == AVERROR(EAGAIN) || av_err == AVERROR_EOF) {
        av_bsf_flush(userdata->bsf);
        while( (av_err = av_bsf_receive_packet(userdata->bsf, userdata->av_packet)) == 0) {
            if(avpacket_to_packet(&userdata->packet,userdata->av_packet) != 0) {
              LOG0("unable to convert packet");
              return -1;
            }

            if( (r = receiver->submit_packet(receiver->handle, &userdata->packet)) != 0) {
                av_strerror(av_err,av_errbuf,sizeof(av_errbuf));
                LOG1("error submitting packet to packet_receiver: %s", av_errbuf);
                return r;
            }
            av_packet_unref(userdata->av_packet);
        }
        if(av_err == AVERROR(EAGAIN) || av_err == AVERROR_EOF) return 1;
    }

    av_strerror(av_err,av_errbuf,sizeof(av_errbuf));
    LOG1("error reading packet from demuxer: %s", av_errbuf);

    LOG1("av_err: %d", av_err);
    LOG1("AVERROR(EAGAIN): %d", AVERROR(EAGAIN));
    return -1;
}

const demuxer_plugin demuxer_plugin_avformat = {
    plugin_name,
    demuxer_plugin_avformat_size,
    demuxer_plugin_avformat_init,
    demuxer_plugin_avformat_deinit,
    demuxer_plugin_avformat_create,
    demuxer_plugin_avformat_config,
    demuxer_plugin_avformat_open,
    demuxer_plugin_avformat_close,
    demuxer_plugin_avformat_run,
};
