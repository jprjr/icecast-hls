#include "demuxer_plugin_avformat.h"

#include "avpacket_utils.h"
#include "avcodec_utils.h"

#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavcodec/bsf.h>

#include <stdio.h>

#define LOG0(fmt) fprintf(stderr, "[demuxer:avformat] " fmt "\n")
#define LOG1(fmt,a) fprintf(stderr, "[demuxer:avformat] " fmt "\n", (a))
#define LOG2(fmt,a,b) fprintf(stderr, "[demuxer:avformat] " fmt "\n", (a), (b))
#define LOG4(fmt,a,b,c,d) fprintf(stderr, "[demuxer:avformat] " fmt "\n", (a), (b), (c), (d))

#define BUFFER_SIZE 4096

static STRBUF_CONST(plugin_name, "avformat");

struct demuxer_plugin_avformat_userdata {
    uint8_t* buffer;
    AVIOContext* io_ctx;
    AVFormatContext* fmt_ctx;
    AVPacket *av_packet;
    AVBSFContext *bsf;
    AVRational tb_src;
    AVRational tb_dest;
#if LIBAVCODEC_VERSION_MAJOR >= 59
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

static int demuxer_plugin_avformat_init(void) {
#if LIBAVFORMAT_VERSION_MAJOR < 58
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
       strbuf_equals_cstr(key,"bsf filters")) {
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

    userdata->buffer    = NULL;
    userdata->io_ctx    = NULL;
    userdata->fmt_ctx   = NULL;
    userdata->codec     = NULL;
    userdata->av_packet = NULL;
    userdata->bsf       = NULL;
    packet_init(&userdata->packet);
    strbuf_init(&userdata->bsf_filters);

    userdata->me = packet_source_zero;

    return 0;
}

static void demuxer_plugin_avformat_close(void* ud) {
    demuxer_plugin_avformat_userdata* userdata = (demuxer_plugin_avformat_userdata*)ud;

    if(userdata->fmt_ctx != NULL) avformat_close_input(&userdata->fmt_ctx);

    if(userdata->io_ctx != NULL) {
        av_free(userdata->io_ctx->buffer);
        av_free(userdata->io_ctx);
        userdata->buffer = NULL;
    }
    if(userdata->buffer != NULL) av_free(userdata->io_ctx);
    if(userdata->av_packet != NULL) {
#if LIBAVCODEC_VERSION_MAJOR < 57
        av_free(userdata->av_packet);
        userdata->av_packet = NULL;
#else
        av_packet_free(&userdata->av_packet);
#endif
    }
    if(userdata->bsf != NULL) {
        av_bsf_free(&userdata->bsf);
    }

    packet_free(&userdata->packet);
    strbuf_free(&userdata->bsf_filters);
    membuf_free(&userdata->me.dsi);
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
    const AVInputFormat *fmt = NULL;

#if LIBAVCODEC_VERSION_MAJOR >= 57
    userdata->av_packet = av_packet_alloc();
#else
    userdata->av_packet = av_mallocz(sizeof(AVPacket));
#endif

    if(userdata->av_packet == NULL) {
        LOG0("failed to allocate packet");
        return -1;
    }

#if LIBAVCODEC_VERSION_MAJOR < 57
    av_init_packet(userdata->av_packet);
#endif

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

static int demuxer_plugin_avformat_run(void* ud, const tag_handler* thandler, const packet_receiver* receiver) {
    demuxer_plugin_avformat_userdata* userdata = (demuxer_plugin_avformat_userdata*)ud;
    char av_errbuf[128];
    int av_err;
    int r;
    unsigned int i;
    AVStream *stream = NULL;

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

        userdata->tb_src = stream->time_base;
        userdata->tb_dest.num = 1;
        userdata->tb_dest.den = stream->codecpar->sample_rate;

        userdata->me.handle = userdata;
        userdata->me.name = &plugin_name;
        userdata->me.sample_rate = stream->codecpar->sample_rate;
        userdata->me.frame_len = stream->codecpar->frame_size;
        userdata->me.padding = stream->codecpar->initial_padding;
        userdata->me.roll_distance = stream->codecpar->seek_preroll;
        userdata->me.sync_flag = 1;

        userdata->me.codec_tag     = stream->codecpar->codec_tag;
        userdata->me.block_align   = stream->codecpar->block_align;
        userdata->me.format   = stream->codecpar->format;
        userdata->me.bit_rate   = stream->codecpar->bit_rate;
        userdata->me.bits_per_coded_sample   = stream->codecpar->bits_per_coded_sample;
        userdata->me.bits_per_raw_sample   = stream->codecpar->bits_per_raw_sample;
        userdata->me.avprofile   = stream->codecpar->profile;
        userdata->me.avlevel   = stream->codecpar->level;
        userdata->me.trailing_padding   = stream->codecpar->trailing_padding;

#if LIBAVUTIL_VERSION_MAJOR >= 58
        switch(stream->codecpar->ch_layout.order) {
            case AV_CHANNEL_ORDER_NATIVE: userdata->me.channel_layout = stream->codecpar->ch_layout.u.mask; break;
            case AV_CHANNEL_ORDER_UNSPEC: {
                switch(stream->codecpar->ch_layout.nb_channels) {
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
                        LOG1("unspecified channel layout, channels=%d",stream->codecpar->ch_layout.nb_channels);
                        return -1;
                    }
                }
                break;
            }
            /* fall-through */
            default: {
                LOG0("unknown channel layout");
                return -1;
            }
        }
#else
        userdata->me.channel_layout = stream->codecpar->channel_layout;
#endif
        userdata->me.codec = avcodec_to_codec(userdata->codec->id);
        if(userdata->me.codec == CODEC_TYPE_UNKNOWN) {
            LOG1("Unknown avcodec id: %u", userdata->codec->id);
            return -1;
        }

        if(userdata->bsf_filters.len > 0) {
            if( (av_err = av_bsf_list_parse_str((const char *)userdata->bsf_filters.x,&userdata->bsf)) < 0) {
                av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
                LOG1("error parsing bsf list: %s", av_errbuf);
                return -1;
            }

        } else {
            if( (av_err = av_bsf_get_null_filter(&userdata->bsf)) < 0) {
                av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
                LOG1("error getting bsf null filter: %s", av_errbuf);
                return -1;
            }
        }

        avcodec_parameters_copy(userdata->bsf->par_in,stream->codecpar);
        userdata->bsf->time_base_in = stream->time_base;

        if( (av_err = av_bsf_init(userdata->bsf)) < 0) {
            av_strerror(av_err,av_errbuf,sizeof(av_errbuf));
            LOG1("error initializing bsf filters: %s", av_errbuf);
            return -1;
        }

        if(stream->codecpar->extradata_size) {
            if( ( r = membuf_append(&userdata->me.dsi,stream->codecpar->extradata,stream->codecpar->extradata_size)) != 0) return r;
        }

        switch(userdata->codec->id) {
            case AV_CODEC_ID_NONE: {
                LOG0("unknown codec");
                return -1;
            }
            case AV_CODEC_ID_AAC: {
                if(stream->codecpar->profile < 0) {
                    /* TODO seekable input for files */
                    LOG0("Unable to detect AAC profile, is this file streamable?");
                    return -1;
                }
                userdata->me.profile = stream->codecpar->profile + 1;
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

        return receiver->open(receiver->handle, &userdata->me);
    }

    stream = userdata->fmt_ctx->streams[userdata->audioStreamIndex];

    if( (av_err = av_read_frame(userdata->fmt_ctx, userdata->av_packet)) == 0) {
        if(userdata->av_packet->stream_index == userdata->audioStreamIndex) {

            if(( av_err = av_bsf_send_packet(userdata->bsf,userdata->av_packet)) != 0) {
                av_strerror(av_err,av_errbuf,sizeof(av_errbuf));
                LOG1("error sending packet to bsf: %s", av_errbuf);
                return -1;
            }

            while( (av_err = av_bsf_receive_packet(userdata->bsf, userdata->av_packet)) == 0) {
                av_packet_rescale_ts(userdata->av_packet, userdata->tb_src, userdata->tb_dest);

                if(avpacket_to_packet(&userdata->packet,userdata->av_packet) != 0) {
                  LOG0("unable to convert packet");
                  return -1;
                }

                /* if we rescaled the sample_rate seems to get overwritten to 1 */
                userdata->packet.sample_rate = userdata->tb_dest.den;

                if( (r = receiver->submit_packet(receiver->handle, &userdata->packet)) != 0) return r;
                av_packet_unref(userdata->av_packet);
            }

            if(av_err == AVERROR(EAGAIN)) return 0;
            if(av_err == AVERROR_EOF) return 1;
            av_strerror(av_err,av_errbuf,sizeof(av_errbuf));
            LOG1("error receiving packet from bsf: %s", av_errbuf);
            return -1;
        }
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
