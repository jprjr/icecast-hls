#include "encoder_plugin.h"

#include "avframe_utils.h"
#include "avpacket_utils.h"

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/dict.h>

#include "pack_u32be.h"

#include <stdlib.h>

#define LOG0(fmt) fprintf(stderr, "[encoder:avcodec] " fmt "\n")
#define LOG1(fmt,a) fprintf(stderr, "[encoder:avcodec] " fmt "\n", (a))
#define LOG2(fmt,a,b) fprintf(stderr, "[encoder:avcodec] " fmt "\n", (a), (b))

#define TRY0(exp, act) if( (r = (exp)) != 0 ) { act; goto cleanup; }
#define TRY(exp, act) if(!(exp)) { act; r = -1; goto cleanup; }


struct plugin_userdata {
    const AVCodec* codec;
    AVCodecContext* ctx;
    AVDictionary* codec_config;
    AVFrame* avframe;
    AVPacket* avpacket;
    packet packet;
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

static int plugin_init(void) {
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static void* plugin_create(void) {
    plugin_userdata* userdata = (plugin_userdata*)malloc(sizeof(plugin_userdata));
    if(userdata == NULL) return NULL;
    userdata->codec = NULL;
    userdata->ctx = NULL;
    userdata->avframe = NULL;
    userdata->avpacket = NULL;
    userdata->codec_config = NULL;
    packet_init(&userdata->packet);
    return userdata;
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    if(userdata->avframe != NULL) av_frame_free(&userdata->avframe);
    if(userdata->avpacket != NULL) av_packet_free(&userdata->avpacket);
    if(userdata->ctx != NULL) avcodec_free_context(&userdata->ctx);
    if(userdata->codec_config != NULL) av_dict_free(&userdata->codec_config);
    packet_free(&userdata->packet);
    free(userdata);
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

static int plugin_open(void* ud, const audioconfig* aconfig, const muxerconfig_handler* mux) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;

    muxerconfig mconfig = MUXERCONFIG_ZERO;
    encoderinfo einfo = ENCODERINFO_ZERO;
    AVDictionaryEntry *t = NULL;
    membuf dsi = STRBUF_ZERO; /* STRBUF_ZERO uses a smaller blocksize */

    if(userdata->codec == NULL) {
        TRY( (userdata->codec = avcodec_find_encoder_by_name("aac")) != NULL,
            LOG0("no encoder specified and unable to find a default"));
    }

    TRY( (userdata->ctx = avcodec_alloc_context3(userdata->codec)) != NULL,
        LOG0("out of memory"));

    userdata->ctx->time_base.num = 1;
    userdata->ctx->time_base.den = aconfig->sample_rate;
    userdata->ctx->sample_rate   = aconfig->sample_rate;

    TRY( (userdata->ctx->sample_fmt = find_best_format(userdata->codec,samplefmt_to_avsampleformat(aconfig->format))) != AV_SAMPLE_FMT_NONE,
        LOG0("unable to find a suitable sample format"));
    av_channel_layout_default(&userdata->ctx->ch_layout,aconfig->channels);

    TRY(avcodec_open2(userdata->ctx, userdata->codec, &userdata->codec_config) >= 0,
        LOG0("unable to open codec context"));

    if(userdata->codec_config != NULL) {
        while( (t = av_dict_get(userdata->codec_config, "", t, AV_DICT_IGNORE_SUFFIX)) != NULL) {
            LOG2("warning: unused option %s=%s",
              t->key, t->value);
        }
    }

    switch(userdata->codec->id) {
        case AV_CODEC_ID_AAC: {
            mconfig.type = CODEC_TYPE_AAC;
            mconfig.roll_distance = -1;
            TRY(userdata->ctx->extradata_size > 0, LOG0("aac missing extradata"));
            TRY0(membuf_append(&dsi, userdata->ctx->extradata, userdata->ctx->extradata_size),
                LOG0("out of memory"));
            break;
        }

        case AV_CODEC_ID_ALAC: {
            mconfig.type = CODEC_TYPE_ALAC;
            TRY(userdata->ctx->extradata_size > 0, LOG0("alac missing extradata"));

            /* ffmpeg's alac encoder includes the mp4 box headers (size, 'alac', flags) */
            TRY(userdata->ctx->extradata_size > 12,
                LOG1("alac extradata too small: %u expected at least 13",
                  userdata->ctx->extradata_size));

            TRY0(membuf_append(&dsi, &userdata->ctx->extradata[12], userdata->ctx->extradata_size - 12),
                LOG0("out of memory"));
            break;
        }

        case AV_CODEC_ID_FLAC: {
            mconfig.type = CODEC_TYPE_FLAC;
            TRY(userdata->ctx->extradata_size > 0, LOG0("flac missing extradata"));

            /* ffmpeg's flac encoder does not include streaminfo's header block */
            TRY(userdata->ctx->extradata_size == 34,
                LOG1("flac extradata size is %u, expected 34",
                  userdata->ctx->extradata_size));

            TRY0(membuf_ready(&dsi,38), LOG0("out of memory"));

            pack_u32be(dsi.x,0x80000000 | 34);
            memcpy(&dsi.x[4],userdata->ctx->extradata, 34);
            dsi.len = 38;
            break;
        }

        case AV_CODEC_ID_MP3: {
            mconfig.type = CODEC_TYPE_MP3;
            mconfig.roll_distance = -1; /* TODO is this right? */
            break;
        }

        default: {
            TRY(0, LOG0("unsupported codec type"));
        }
    }

    mconfig.channels = aconfig->channels;
    mconfig.sample_rate = aconfig->sample_rate;
    mconfig.frame_len = userdata->ctx->frame_size;
    mconfig.sync_flag = userdata->ctx->codec_descriptor->props & AV_CODEC_PROP_INTRA_ONLY;
    mconfig.padding = userdata->ctx->initial_padding;
    mconfig.info = muxerinfo_ignore;

    TRY0(mux->submit(mux->userdata, &mconfig), LOG0("error configuring muxer"));

    TRY0(mux->submit_dsi(mux->userdata, &dsi), LOG0("error: unable to submit dsi to muxer"));

    TRY( (userdata->avframe = av_frame_alloc()) != NULL, LOG0("out of memory"));
    TRY( (userdata->avpacket = av_packet_alloc()) != NULL, LOG0("out of memory"));

    cleanup:
    membuf_free(&dsi);
    if(r != 0) return r;

    /* let's make sure we can get input in our format */
    einfo.format = avsampleformat_to_samplefmt(find_best_format(userdata->codec,samplefmt_to_avsampleformat(aconfig->format)));
    einfo.frame_len = userdata->ctx->frame_size;

    return aconfig->info.submit(aconfig->info.userdata, &einfo);

}

static int drain_packets(plugin_userdata* userdata, const packet_handler* mux, int flushing) {
    int av;

    while( (av = avcodec_receive_packet(userdata->ctx, userdata->avpacket)) >= 0) {

        if(userdata->avpacket->duration == 0) {
            if(!flushing) {
                LOG0("error, received a packet with zero duration");
                return AVERROR_EXTERNAL;
            }
            return AVERROR_EOF;
        }

        if(avpacket_to_packet(&userdata->packet,userdata->avpacket) != 0) {
          LOG0("unable to convert packet");
          return AVERROR_EXTERNAL;
        }

        if(mux->cb(mux->userdata, &userdata->packet) != 0) {
          LOG0("unable to send packet");
          return AVERROR_EXTERNAL;
        }

    }

    return av;
}

static int plugin_submit_frame(void* ud, const frame* frame, const packet_handler* mux) {
    int r;
    int av;
    char averrbuf[128];
    plugin_userdata* userdata = (plugin_userdata*)ud;

    TRY0(frame_to_avframe(userdata->avframe,frame),
      LOG0("unable to convert frame"));

    TRY( (av = avcodec_send_frame(userdata->ctx,userdata->avframe)) >= 0,
      av_strerror(av, averrbuf, sizeof(averrbuf));
      LOG1("unable to send frame: %s",averrbuf));

    TRY( (av = drain_packets(userdata,mux, 0)) == AVERROR(EAGAIN),
      av_strerror(av, averrbuf, sizeof(averrbuf));
      LOG1("frame: error receiving packet: %s",averrbuf));

    cleanup:
    return r;
}

static int plugin_flush(void* ud, const packet_handler* mux) {
    int r;
    int av;
    char averrbuf[128];
    plugin_userdata* userdata = (plugin_userdata*)ud;

    TRY( (av = avcodec_send_frame(userdata->ctx,NULL)) >= 0,
      av_strerror(av, averrbuf, sizeof(averrbuf));
      LOG1("unable to flush encoder: %s",averrbuf));

    TRY( (av = drain_packets(userdata,mux,1)) == AVERROR_EOF,
      av_strerror(av, averrbuf, sizeof(averrbuf));
      LOG1("flush: error receiving packet: %s",averrbuf));
    r = 0;

    cleanup:

    return r == 0 ? mux->flush(mux->userdata) : r;
}

const encoder_plugin encoder_plugin_avcodec = {
    { .a = 0, .len = 7, .x = (uint8_t*)"avcodec" },
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_submit_frame,
    plugin_flush,
};

