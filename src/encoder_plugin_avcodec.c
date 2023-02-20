#include "encoder_plugin.h"

#include "avframe_utils.h"
#include "avpacket_utils.h"

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>

#include "pack_u32be.h"
#include "pack_u16be.h"
#include "unpack_u32le.h"
#include "unpack_u16le.h"

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

static int plugin_open(void* ud, const frame_source* source, const packet_receiver *dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;
    uint32_t tmp;
    uint32_t u;

    packet_source me = PACKET_SOURCE_ZERO;
    frame_source_params params = FRAME_SOURCE_PARAMS_ZERO;
    AVDictionaryEntry *t = NULL;
    membuf dsi = STRBUF_ZERO; /* STRBUF_ZERO uses a smaller blocksize */

    if(userdata->codec == NULL) {
        TRY( (userdata->codec = avcodec_find_encoder_by_name("aac")) != NULL,
            LOG0("no encoder specified and unable to find a default"));
    }

    TRY( (userdata->ctx = avcodec_alloc_context3(userdata->codec)) != NULL,
        LOG0("out of memory"));

    userdata->ctx->time_base.num = 1;
    userdata->ctx->time_base.den = source->sample_rate;
    userdata->ctx->sample_rate   = source->sample_rate;

    TRY( (userdata->ctx->sample_fmt = find_best_format(userdata->codec,samplefmt_to_avsampleformat(source->format))) != AV_SAMPLE_FMT_NONE,
        LOG0("unable to find a suitable sample format"));
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57,28,100)
    av_channel_layout_default(&userdata->ctx->ch_layout,source->channels);
#else
    userdata->ctx->channel_layout = av_get_default_channel_layout(source->channels);
#endif

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
            me.codec = CODEC_TYPE_AAC;
            me.profile = userdata->ctx->profile + 1;
            me.roll_distance = -1;
            TRY(userdata->ctx->extradata_size > 0, LOG0("aac missing extradata"));
            TRY0(membuf_append(&dsi, userdata->ctx->extradata, userdata->ctx->extradata_size),
                LOG0("out of memory"));
            break;
        }

        case AV_CODEC_ID_ALAC: {
            me.codec = CODEC_TYPE_ALAC;
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
            me.codec = CODEC_TYPE_FLAC;
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
            me.codec = CODEC_TYPE_MP3;
            break;
        }

        case AV_CODEC_ID_AC3: {
            me.codec = CODEC_TYPE_AC3;
            TRY0(membuf_ready(&dsi,4), LOG0("out of memory"));

            switch(source->sample_rate) {
                case 48000: {
                    tmp = 0; break;
                }
                case 44100: {
                    tmp = 1; break;
                }
                case 32000: {
                    tmp = 2; break;
                }
                default: {
                    LOG1("ac3: unsupported sample rate %u",
                      source->sample_rate);
                    return -1;
                }
            }

            /* dsi is:
               fscod, 2 bits
               bsid, 5 bits
               bsmod, 3 bits
               acmod, 3 bits
               lfeon, 1 bit
               bit_rate_code, 5 bits
               reserved, 5 bits */

            tmp <<= (32 - 2);

            /* bsid 8 = current AC3 standard */
            tmp |= 8 << (32 - 2 - 5);

            /* bsmod, 3 bits */
            tmp |= 0 << (32 - 2 - 5 - 3);

            /* acmod, 3 bits */
            /* 001 = mono
             * 002 = stereo */
            tmp |= source->channels << (32 - 2 - 5 - 3 - 3);

            /* lfeon, 1 bit, always false */
            tmp |= 0 << (32 - 2 - 5 - 3 - 3 - 1);

            /* bit_rate_code TODO */
            u = 0;
            switch(userdata->ctx->bit_rate) {
                case 640000: u++; /* fall-through */
                case 576000: u++; /* fall-through */
                case 512000: u++; /* fall-through */
                case 448000: u++; /* fall-through */
                case 384000: u++; /* fall-through */
                case 320000: u++; /* fall-through */
                case 256000: u++; /* fall-through */
                case 224000: u++; /* fall-through */
                case 192000: u++; /* fall-through */
                case 160000: u++; /* fall-through */
                case 128000: u++; /* fall-through */
                case 112000: u++; /* fall-through */
                case 96000:  u++; /* fall-through */
                case 80000:  u++; /* fall-through */
                case 64000:  u++; /* fall-through */
                case 56000:  u++; /* fall-through */
                case 48000:  u++; /* fall-through */
                case 40000:  u++; /* fall-through */
                case 32000:  break;
                default: {
                    /* hard-code to 192 */
                    u = 0x0a;
                    break;
                }
            }

            tmp |= u << (32 - 2 - 5 - 3 - 3 - 1 - 5);

            /* reserved */
            tmp |= 0 << (32 - 2 - 5 - 3 - 3 - 1 - 5 - 5);

            pack_u32be(dsi.x,tmp);
            dsi.len = 3;
            break;
        }

        case AV_CODEC_ID_EAC3: {
            me.codec = CODEC_TYPE_EAC3;

            TRY0(membuf_ready(&dsi,8), LOG0("out of memory"));

            /* dsi is:
               data_rate, 13 bits
               num_ind_sub, 3 bits
               for each independent substream (only 1 for this app):
                 fscod, 2 bits
                 bsid, 5 bits
                 reserved, 1 bit
                 asvc, 1 bit
                 bsmod, 3 bits
                 acmod, 3 bits
                 lfeon, 1 bit
                 reserved, 3 bits
                 num_dep_sub, 4 bits
                 if num_dep_sub > 0:
                      chan_loc, 9 bits
                 else:
                     reserved, 1 bit
               */

            if(userdata->ctx->bit_rate > 0) {
                tmp = userdata->ctx->bit_rate / 1000;
            } else {
                tmp = 192; /* just hard-code as 192kbps */
            }

            tmp <<= 16 - 13;
            /* num_ind_sub, hard-code to 0 */
            tmp |= 0 << (16 - 13 - 3);
            pack_u16be(&dsi.x[0],(uint16_t)tmp);

            switch(source->sample_rate) {
                case 48000: {
                    tmp = 0; break;
                }
                case 44100: {
                    tmp = 1; break;
                }
                case 32000: {
                    tmp = 2; break;
                }
                default: {
                    LOG1("eac3: unsupported sample rate %u",
                      source->sample_rate);
                    return -1;
                }
            }

            tmp <<= (32 - 2);

            /* bsid 16 = current EAC3 standard */
            tmp |= 8 << (32 - 2 - 5);

            /* reserved 1 bit */
            tmp |= 0 << (32 - 2 - 5 - 1);

            /* asvc 1 bit */
            tmp |= 0 << (32 - 2 - 5 - 1 - 1);

            /* bsmod, 3 bits */
            tmp |= 0 << (32 - 2 - 5 - 1 - 1 - 3);

            /* acmod, 3 bits */
            /* 001 = mono
             * 002 = stereo */
            tmp |= source->channels << (32 - 2 - 5 - 1 - 1 - 3 - 3);

            /* lfeon, 1 bit, always false */
            tmp |= 0 << (32 - 2 - 5 - 1 - 1 - 3 - 3 - 1);

            /* 3 reserved bits */
            tmp |= 0 << (32 - 2 - 5 - 1 - 1 - 3 - 3 - 1 - 3);

            /* num_dep_sub 4 bits */
            tmp |= 0 << (32 - 2 - 5 - 1 - 1 - 3 - 3 - 1 - 3 - 4);

            /* reserved 1 bit */
            tmp |= 0 << (32 - 2 - 5 - 1 - 1 - 3 - 3 - 1 - 3 - 4 - 1);

            pack_u32be(&dsi.x[2],tmp);
            dsi.len = 5;
            break;
        }

        case AV_CODEC_ID_OPUS: {
            me.codec = CODEC_TYPE_OPUS;
            /* opus specs states you need (at least) 80ms of preroll,
             * which is 3840 samples @48kHz */
            me.roll_distance = -3840 / userdata->ctx->frame_size;

            /* the extradata from avcodec is the pull header packet including OpusHead,
             * so we just copy it in and swap a few bytes (OpusHead uses little-endian,
             * mp4 uses big-endian */

            TRY(userdata->ctx->extradata_size > 8,
                LOG1("opus extradatasize is %u, expected at least 9",
                  userdata->ctx->extradata_size));
            TRY0(membuf_ready(&dsi,userdata->ctx->extradata_size-8), LOG0("out of memory"));
            memcpy(dsi.x,&userdata->ctx->extradata[8],userdata->ctx->extradata_size-8);
            dsi.len = userdata->ctx->extradata_size-8;

            dsi.x[0] = 0x00;
            pack_u16be(&dsi.x[2],unpack_u16le(&dsi.x[2]));
            pack_u32be(&dsi.x[4],unpack_u32le(&dsi.x[4]));
            pack_u16be(&dsi.x[8],unpack_u16le(&dsi.x[8]));
            break;
        }

        default: {
            TRY(0, LOG0("unsupported codec type"));
        }
    }

    params.format   = avsampleformat_to_samplefmt(find_best_format(userdata->codec,samplefmt_to_avsampleformat(source->format)));
    params.duration = userdata->ctx->frame_size;

    me.channels = source->channels;
    me.sample_rate = source->sample_rate;
    me.frame_len = userdata->ctx->frame_size;
    me.sync_flag = userdata->ctx->codec_descriptor->props & AV_CODEC_PROP_INTRA_ONLY;
    me.padding = userdata->ctx->initial_padding;
    me.set_params = packet_source_set_params_ignore;
    if(me.frame_len == 0) me.frame_len = 1024;

    TRY0(dest->open(dest->handle, &me), LOG0("error configuring muxer"));

    TRY0(dest->submit_dsi(dest->handle, &dsi), LOG0("error: unable to submit dsi to muxer"));

    TRY( (userdata->avframe = av_frame_alloc()) != NULL, LOG0("out of memory"));
    TRY( (userdata->avpacket = av_packet_alloc()) != NULL, LOG0("out of memory"));

    TRY0(source->set_params(source->handle, &params),LOG0("error setting source params"));

    cleanup:
    membuf_free(&dsi);
    return r;

}

static int drain_packets(plugin_userdata* userdata, const packet_receiver* dest, int flushing) {
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

        if(dest->submit_packet(dest->handle, &userdata->packet) != 0) {
          LOG0("unable to send packet");
          return AVERROR_EXTERNAL;
        }

    }

    return av;
}

static int plugin_submit_frame(void* ud, const frame* frame, const packet_receiver* dest) {
    int r;
    int av;
    char averrbuf[128];
    plugin_userdata* userdata = (plugin_userdata*)ud;

    TRY0(frame_to_avframe(userdata->avframe,frame),
      LOG0("unable to convert frame"));

    TRY( (av = avcodec_send_frame(userdata->ctx,userdata->avframe)) >= 0,
      av_strerror(av, averrbuf, sizeof(averrbuf));
      LOG1("unable to send frame: %s",averrbuf));

    TRY( (av = drain_packets(userdata,dest, 0)) == AVERROR(EAGAIN),
      av_strerror(av, averrbuf, sizeof(averrbuf));
      LOG1("frame: error receiving packet: %s",averrbuf));

    cleanup:
    return r;
}

static int plugin_flush(void* ud, const packet_receiver* dest) {
    int r;
    int av;
    char averrbuf[128];
    plugin_userdata* userdata = (plugin_userdata*)ud;

    TRY( (av = avcodec_send_frame(userdata->ctx,NULL)) >= 0,
      av_strerror(av, averrbuf, sizeof(averrbuf));
      LOG1("unable to flush encoder: %s",averrbuf));

    TRY( (av = drain_packets(userdata,dest,1)) == AVERROR_EOF,
      av_strerror(av, averrbuf, sizeof(averrbuf));
      LOG1("flush: error receiving packet: %s",averrbuf));
    r = 0;

    cleanup:

    return r == 0 ? dest->flush(dest->handle) : r;
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

