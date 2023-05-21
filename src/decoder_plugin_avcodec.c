#include "decoder_plugin_avcodec.h"

#include "avframe_utils.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/mem.h>

#define LOG0(fmt) fprintf(stderr, "[decoder:avcodec] " fmt "\n")
#define LOG1(fmt,a) fprintf(stderr, "[decoder:avcodec] " fmt "\n", (a))
#define LOG2(fmt,a,b) fprintf(stderr, "[decoder:avcodec] " fmt "\n", (a), (b))
#define LOG4(fmt,a,b,c,d) fprintf(stderr, "[decoder:avcodec] " fmt "\n", (a), (b), (c), (d))

#define BUFFER_SIZE 4096

struct decoder_plugin_avcodec_userdata {
    uint8_t* buffer;
    AVIOContext* io_ctx;
    AVFormatContext* fmt_ctx;
    AVCodecContext *codec_ctx;
    AVPacket *packet;
    AVFrame *avframe;
#if LIBAVCODEC_VERSION_MAJOR >= 59
    const AVCodec* codec;
#else
    AVCodec* codec;
#endif
    int audioStreamIndex;
    frame frame;
    taglist list;
    strbuf tmpstr;
    uint8_t tags_type; /* 0 = ignore, 1 = stream, 2 = format */
};

typedef struct decoder_plugin_avcodec_userdata decoder_plugin_avcodec_userdata;

static int decoder_plugin_avcodec_init(void) {
#if LIBAVFORMAT_VERSION_MAJOR < 58
    av_register_all();
#endif
#if LIBAVCODEC_VERSION_MAJOR < 58
    avcodec_register_all();
#endif
    return 0;
}

static void decoder_plugin_avcodec_deinit(void) {
    return;
}

static void* decoder_plugin_avcodec_create(void) {
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)malloc(sizeof(decoder_plugin_avcodec_userdata));
    if(userdata == NULL) return NULL;

    userdata->buffer    = NULL;
    userdata->io_ctx    = NULL;
    userdata->fmt_ctx   = NULL;
    userdata->codec     = NULL;
    userdata->codec_ctx = NULL;
    userdata->packet    = NULL;
    userdata->avframe   = NULL;
    userdata->tags_type = 1; /* default to stream tags */

    frame_init(&userdata->frame);
    taglist_init(&userdata->list);
    strbuf_init(&userdata->tmpstr);

    return userdata;
}

static int decoder_plugin_avcodec_tags(decoder_plugin_avcodec_userdata* userdata, AVDictionary **dict) {
    const AVDictionaryEntry *tag = NULL;
    strbuf val;
    int r;
    size_t keylen, vallen;

    taglist_reset(&userdata->list);
    while( (tag = av_dict_get(*dict, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        if( (keylen = strlen(tag->key)) == 0) continue;
        if( (vallen = strlen(tag->value)) == 0) continue;

        if( (r = strbuf_ready(&userdata->tmpstr,keylen)) != 0) {
            LOG1("out of memory line %u",__LINE__);
            abort();
        }
        memcpy(userdata->tmpstr.x,tag->key,keylen);
        userdata->tmpstr.len = keylen;
        strbuf_lower(&userdata->tmpstr);
        val.x = (uint8_t *)tag->value;
        val.len = vallen;

        if( (r = taglist_add(&userdata->list,&userdata->tmpstr,&val)) != 0) return r;
    }
    av_dict_free(dict);
    return 0;
}


static int decoder_plugin_avcodec_config(void* ud, const strbuf* key, const strbuf* value) {
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)ud;

    if(strbuf_equals_cstr(key,"ignore tags") || strbuf_equals_cstr(key,"ignore-tags")) {
        if(strbuf_truthy(value)) {
            userdata->tags_type = 0;
            return 0;
        }
        if(strbuf_falsey(value)) {
            if(userdata->tags_type == 0) {
                userdata->tags_type = 1; /* assume we want stream tags */
            }
            return 0;
        }
        LOG4("unknown value for key %.*s: %.*s\n",
          (int)key->len,(char *)key->x,(int)value->len,(char *)value->x);
        return -1;
    }

    if(strbuf_equals_cstr(key,"tags type") || strbuf_equals_cstr(key,"tags-type")) {
        if(strbuf_casecontains_cstr(value,"none")) {
            userdata->tags_type = 0;
            return 0;
        }
        if(strbuf_casecontains_cstr(value,"stream")) {
            userdata->tags_type = 1;
            return 0;
        }
        if(strbuf_casecontains_cstr(value,"format")) {
            userdata->tags_type = 2;
            return 0;
        }
        LOG4("unknown value for key %.*s: %.*s\n",
          (int)key->len,(char *)key->x,(int)value->len,(char *)value->x);
        return -1;
    }

    return 0;
}

static int decoder_plugin_avcodec_read(void *ud, uint8_t *buf, int buf_size) {
    input* in = (input*)ud;
    size_t r = input_read(in, buf, (size_t)buf_size);

	if(r == 0) return AVERROR_EOF;
    return (int)r;
}

static int decoder_plugin_avcodec_handle_source_params(void* ud, const frame_source_params* params) {
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)ud;

    if(params->format == SAMPLEFMT_UNKNOWN) return 0;
    if(params->format == userdata->frame.format) return 0;

    LOG0("an upstream source is trying to change our format");
    return -1;
}

static int decoder_plugin_avcodec_open(void* ud, input* in, const frame_receiver* dest) {
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)ud;
    frame_source me = FRAME_SOURCE_ZERO;
    char av_errbuf[128];
    int av_err;
    unsigned int i;

#if LIBAVCODEC_VERSION_MAJOR >= 57
    userdata->packet = av_packet_alloc();
#else
    userdata->packet = av_mallocz(sizeof(AVPacket));
#endif

    if(userdata->packet == NULL) {
        LOG0("failed to allocate packet");
        return -1;
    }

    userdata->avframe = av_frame_alloc();
    if(userdata->avframe == NULL) {
        LOG0("failed to allocate avframe");
        return -1;
    }

#if LIBAVCODEC_VERSION_MAJOR < 57
    av_init_packet(userdata->packet);
#endif

    userdata->buffer = av_malloc(BUFFER_SIZE + AVPROBE_PADDING_SIZE);
    if(userdata->buffer == NULL) {
        LOG0("failed to allocate buffer");
        return -1;
    }
    userdata->io_ctx = avio_alloc_context(userdata->buffer, BUFFER_SIZE,
      0, /* write flag */
      (void *)in,
      decoder_plugin_avcodec_read,
      NULL,
      NULL);

    if(userdata->io_ctx == NULL) {
        LOG0("failed to allocate io context");
        return -1;
    }

    userdata->fmt_ctx = avformat_alloc_context();
    if(userdata->fmt_ctx == NULL) {
        LOG0("failed to allocate format context");
        return -1;
    }

    userdata->fmt_ctx->pb = userdata->io_ctx;
    userdata->fmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

    if( (av_err = avformat_open_input(&userdata->fmt_ctx, "", NULL, NULL)) < 0) {
        av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
        LOG1("error with avformat_open_input: %s", av_errbuf);
        return -1;
    }

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

    userdata->fmt_ctx->streams[userdata->audioStreamIndex]->discard = AVDISCARD_DEFAULT;

    userdata->codec_ctx = avcodec_alloc_context3(userdata->codec);
    if(userdata->codec_ctx == NULL) {
        LOG0("error allocating AVCodecContext");
        return -1;
    }
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57,41,0) /* ffmpeg-3.1 */
    avcodec_parameters_to_context(userdata->codec_ctx,userdata->fmt_ctx->streams[userdata->audioStreamIndex]->codecpar);
#else
    if( (av_err = avcodec_copy_context(userdata->codec_ctx, userdata->fmt_ctx->streams[userdata->audioStreamIndex]->codec)) != 0) {
        av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
        LOG1("error with avcodec_copy_context: %s", av_errbuf);
        return -1;
    }
#endif

    if( (av_err = avcodec_open2(userdata->codec_ctx, userdata->codec, NULL)) < 0) {
        av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
        LOG1("error with avcodec_open2: %s", av_errbuf);
        return -1;
    }

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

    me.channels = userdata->frame.channels;
    me.format = userdata->frame.format;
    me.sample_rate = userdata->codec_ctx->sample_rate;

    me.set_params = decoder_plugin_avcodec_handle_source_params;
    me.handle = userdata;

    if(dest->open(dest->handle, &me) != 0) {
        LOG0("error opening audio destination");
        return -1;
    }

    /* check for any format tags */
    switch(userdata->tags_type) {
        case 1: /* stream tags */ {
            if(userdata->fmt_ctx->streams[userdata->audioStreamIndex]->event_flags & AVSTREAM_EVENT_FLAG_METADATA_UPDATED) {
                if(decoder_plugin_avcodec_tags(userdata, &userdata->fmt_ctx->streams[userdata->audioStreamIndex]->metadata) != 0) return -1;
                userdata->fmt_ctx->streams[userdata->audioStreamIndex]->event_flags &= ~AVSTREAM_EVENT_FLAG_METADATA_UPDATED;
            }

            break;
        }
        case 2: /* format tags */ {
            if(userdata->fmt_ctx->event_flags & AVSTREAM_EVENT_FLAG_METADATA_UPDATED) {
                if(decoder_plugin_avcodec_tags(userdata, &userdata->fmt_ctx->metadata) != 0) return -1;
                userdata->fmt_ctx->event_flags &= ~AVSTREAM_EVENT_FLAG_METADATA_UPDATED;
            }

        }
        default: break;
    }

    return 0;
}

static void decoder_plugin_avcodec_close(void* ud) {
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)ud;

    if(userdata->fmt_ctx != NULL) avformat_close_input(&userdata->fmt_ctx);
    if(userdata->codec_ctx != NULL) avcodec_free_context(&userdata->codec_ctx);

    if(userdata->io_ctx != NULL) {
        av_free(userdata->io_ctx->buffer);
        av_free(userdata->io_ctx);
        userdata->buffer = NULL;
    }
    if(userdata->buffer != NULL) av_free(userdata->io_ctx);
    if(userdata->avframe != NULL) av_frame_free(&userdata->avframe);

    frame_free(&userdata->frame);
    taglist_free(&userdata->list);
    strbuf_free(&userdata->tmpstr);
    free(userdata);
}

static int decoder_plugin_avcodec_decode(void* ud, const tag_handler* tag_handler, const frame_receiver* frame_dest) {
    int r;
    decoder_plugin_avcodec_userdata* userdata = (decoder_plugin_avcodec_userdata*)ud;
    int av_err;
    AVFrame* frame;
    char av_errbuf[128];
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57,48,0) /* ffmpeg-3.1 */
    int got;
#endif

    frame = av_frame_alloc();
    if(frame == NULL) {
        LOG0("error allocating frame");
        return -1;
    }

    if(userdata->tags_type && taglist_len(&userdata->list)) {
        if( (r = tag_handler->cb(tag_handler->userdata, &userdata->list)) != 0) return r;
    }

    tryagain:
    if( (av_err = av_read_frame(userdata->fmt_ctx, userdata->packet)) == 0) {
        switch(userdata->tags_type) {
            case 1: /* stream tags */ {
                if(userdata->fmt_ctx->streams[userdata->audioStreamIndex]->event_flags & AVSTREAM_EVENT_FLAG_METADATA_UPDATED) {
                    if(decoder_plugin_avcodec_tags(userdata, &userdata->fmt_ctx->streams[userdata->audioStreamIndex]->metadata) != 0) return -1;
                    userdata->fmt_ctx->streams[userdata->audioStreamIndex]->event_flags &= ~AVSTREAM_EVENT_FLAG_METADATA_UPDATED;
                    if( (r = tag_handler->cb(tag_handler->userdata, &userdata->list)) != 0) return r;
                }

                break;
            }
            case 2: /* format tags */ {
                if(userdata->fmt_ctx->event_flags & AVSTREAM_EVENT_FLAG_METADATA_UPDATED) {
                    if(decoder_plugin_avcodec_tags(userdata, &userdata->fmt_ctx->metadata) != 0) return -1;
                    userdata->fmt_ctx->event_flags &= ~AVSTREAM_EVENT_FLAG_METADATA_UPDATED;
                    if( (r = tag_handler->cb(tag_handler->userdata, &userdata->list)) != 0) return r;
                }

            }
            default: break;
        }

        if(userdata->packet->stream_index == userdata->audioStreamIndex) {
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,0) /* ffmpeg-3.1 */
            if( (av_err = avcodec_send_packet(userdata->codec_ctx, userdata->packet)) != 0) {
                av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
                LOG1("error with avcodec_send_packet: %s", av_errbuf);
                return -1;
            }

            av_err = avcodec_receive_frame(userdata->codec_ctx, frame);
            if(av_err == 0) {
#else
            if( (av_err = avcodec_decode_audio4(userdata->codec_ctx, frame, &got, userdata->packet)) < 0) {
                av_strerror(av_err, av_errbuf, sizeof(av_errbuf));
                LOG1("error with avcodec_decode_audio4: %s", av_errbuf);
                return -1;
            }
            av_err = 0;
            if(got) {
#endif
                av_err = avframe_to_frame(&userdata->frame, frame);
                av_frame_unref(frame);
                if(av_err != 0) {
                    LOG0("error converting avframe to frame");
                    return -1;
                }
                if(frame_dest->submit_frame(frame_dest->handle,&userdata->frame) != 0) {
                    LOG0("error submitting audio frame");
                    return -1;
                }
            }
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,0) /* ffmpeg-3.1 */
            else {
                if(av_err == AVERROR(EAGAIN)) goto tryagain;
                return -1;
            }
#endif
        }
        av_packet_unref(userdata->packet);
        return 0;
    }

    if(av_err == AVERROR_EOF) {
        if(frame_dest->flush(frame_dest->handle) == 0) return 1;
    }
    return -1;
}

const decoder_plugin decoder_plugin_avcodec = {
    { .a = 0, .len = 8, .x = (uint8_t *)"avcodec" },
    decoder_plugin_avcodec_init,
    decoder_plugin_avcodec_deinit,
    decoder_plugin_avcodec_create,
    decoder_plugin_avcodec_config,
    decoder_plugin_avcodec_open,
    decoder_plugin_avcodec_close,
    decoder_plugin_avcodec_decode,
};
