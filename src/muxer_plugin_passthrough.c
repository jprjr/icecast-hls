#include "muxer_plugin_passthrough.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define LOG0(str) fprintf(stderr,"[muxer:passthrough] "str"\n")
#define LOG1(s, a) fprintf(stderr,"[muxer:passthrough] "s"\n", (a))

#define LOGERRNO(s) LOG1(s": %s", strerror(errno))
#define TRY(exp, act) if(!(exp)) { act; }
#define TRYNULL(exp, act) if((exp) == NULL) { act; }

static STRBUF_CONST(mime_aac,"audio/aac");
static STRBUF_CONST(mime_mp3,"audio/mpeg");
static STRBUF_CONST(mime_ac3,"audio/ac3");
static STRBUF_CONST(mime_eac3,"audio/eac3");

static STRBUF_CONST(ext_aac,".aac");
static STRBUF_CONST(ext_mp3,".mp3");
static STRBUF_CONST(ext_ac3,".ac3");
static STRBUF_CONST(ext_eac3,".eac3");

struct muxer_plugin_passthrough_userdata {
    uint8_t profile;
    uint8_t freq;
    uint8_t ch_index;
    membuf packet;
    int (*prefix_packet)(struct muxer_plugin_passthrough_userdata*, const packet*);
};
typedef struct muxer_plugin_passthrough_userdata muxer_plugin_passthrough_userdata;

static int prefix_packet_passthrough(struct muxer_plugin_passthrough_userdata* userdata, const packet* p) {
    int r;
    if( (r = membuf_copy(&userdata->packet,&p->data)) != 0) {
        LOGERRNO("error appending packet");
    }
    return r;
}

static int prefix_packet_adts(struct muxer_plugin_passthrough_userdata* userdata, const packet* p) {
    int r;
    uint8_t adts_header[7];
    adts_header[0] = 0xFF;
    adts_header[1] = 0xF1;
    adts_header[2] = 0x00;
    adts_header[2] |= (userdata->profile & 0x03) << 6;
    adts_header[2] |= (userdata->freq & 0x0F) << 2;
    adts_header[2] |= (userdata->ch_index & 0x04) >> 2;
    adts_header[3] = 0x00;
    adts_header[3] |= (userdata->ch_index & 0x03) << 6;
    adts_header[3] |= ( (7 + p->data.len) & 0x1800) >> 11;
    adts_header[4] = 0x00;
    adts_header[4] |= ( (7 + p->data.len) & 0x07F8) >> 3;
    adts_header[5] = 0x00;
    adts_header[5] |= ( (7 + p->data.len) & 0x0007) << 5;
    adts_header[5] |= 0x1F;
    adts_header[6] = 0xFC;

    userdata->packet.len = 0;
    if( (r = membuf_append(&userdata->packet,adts_header,7)) != 0) {
        LOGERRNO("error appending packet");
    }

    if( (r = membuf_cat(&userdata->packet,&p->data)) != 0) {
        LOGERRNO("error appending packet");
    }
    return r;
}

static void* muxer_plugin_passthrough_create(void) {
    muxer_plugin_passthrough_userdata* userdata = NULL;
    TRYNULL(userdata = (muxer_plugin_passthrough_userdata*)malloc(sizeof(muxer_plugin_passthrough_userdata)),
      LOGERRNO("error allocating plugin"); abort());
    userdata->profile = 0;
    userdata->freq = 0;
    userdata->ch_index = 0;
    membuf_init(&userdata->packet);
    userdata->prefix_packet = NULL;

    return userdata;
}

static void muxer_plugin_passthrough_close(void* ud) {
    muxer_plugin_passthrough_userdata* userdata = (muxer_plugin_passthrough_userdata*)ud;

    membuf_free(&userdata->packet);
    free(userdata);
}

static int muxer_plugin_passthrough_open(void* ud, const packet_source* source, const segment_receiver* dest) {
    int r;
    muxer_plugin_passthrough_userdata* userdata = (muxer_plugin_passthrough_userdata*)ud;

    segment_source me = SEGMENT_SOURCE_ZERO;
    packet_source_params params = PACKET_SOURCE_PARAMS_ZERO;

    unsigned int sample_rate = source->sample_rate;
    unsigned int channels = source->channels;
    unsigned int profile = source->profile;

    switch(source->codec) {
        case CODEC_TYPE_AAC: {
            switch(profile) {
                case CODEC_PROFILE_AAC_LC: break;
                case CODEC_PROFILE_AAC_HE2: channels = 1; /* fall-through */
                case CODEC_PROFILE_AAC_HE: sample_rate /= 2; profile = CODEC_PROFILE_AAC_LC; break;
                case CODEC_PROFILE_AAC_USAC: /* fall-through */
                default: {
                    LOG1("unsupported AAC profile %u",source->profile);
                    return -1;
                }
            }

            switch(sample_rate) {
                case 96000: userdata->freq = 0x00; break;
                case 88200: userdata->freq = 0x01; break;
                case 64000: userdata->freq = 0x02; break;
                case 48000: userdata->freq = 0x03; break;
                case 44100: userdata->freq = 0x04; break;
                case 32000: userdata->freq = 0x05; break;
                case 24000: userdata->freq = 0x06; break;
                case 22050: userdata->freq = 0x07; break;
                case 16000: userdata->freq = 0x08; break;
                case 12000: userdata->freq = 0x09; break;
                case 11025: userdata->freq = 0x0A; break;
                case  8000: userdata->freq = 0x0B; break;
                case  7350: userdata->freq = 0x0C; break;
                default: {
                    LOG1("unsupported sample rate %u",sample_rate);
                    return -1;
                }
            }

            switch(channels) {
                case 1: /* fall-through */
                case 2: userdata->ch_index = channels; break;
                default: {
                    LOG1("unsupported channel count %u", channels);
                    return -1;
                }
            }
            userdata->prefix_packet = prefix_packet_adts;
            userdata->profile = profile - 1;
            me.media_ext = &ext_aac;
            me.media_mimetype = &mime_aac;
            break;
        }
        case CODEC_TYPE_MP3: {
            userdata->prefix_packet = prefix_packet_passthrough;
            me.media_ext = &ext_mp3;
            me.media_mimetype = &mime_mp3;
            break;
        }

        case CODEC_TYPE_AC3: {
            userdata->prefix_packet = prefix_packet_passthrough;
            me.media_ext = &ext_ac3;
            me.media_mimetype = &mime_ac3;
            break;
        }

        case CODEC_TYPE_EAC3: {
            userdata->prefix_packet = prefix_packet_passthrough;
            me.media_ext = &ext_eac3;
            me.media_mimetype = &mime_eac3;
            break;
        }

        default: {
            LOG1("unsupported codec %s", codec_name(source->codec));
            return -1;
        }
    }

    me.time_base = source->sample_rate;
    me.frame_len = source->frame_len;
    me.handle = userdata;
    me.set_params = segment_source_set_params_ignore;

    if( (r = dest->open(dest->handle, &me)) != 0) return r;

    params.packets_per_segment = 1;

    return source->set_params(source->handle, &params);
}

static int muxer_plugin_passthrough_submit_packet(void* ud, const packet* packet, const segment_receiver* dest) {
    int r;
    muxer_plugin_passthrough_userdata* userdata = (muxer_plugin_passthrough_userdata*)ud;

    if( (r = userdata->prefix_packet(userdata,packet)) != 0) return r;

    segment s;

    s.type = SEGMENT_TYPE_MEDIA;
    s.data = userdata->packet.x;
    s.len  = userdata->packet.len;
    s.samples = packet->duration;
    s.pts = packet->pts;
    return dest->submit_segment(dest->handle,&s);
}

static int muxer_plugin_passthrough_flush(void* ud, const segment_receiver* dest) {
    (void)ud;
    return dest->flush(dest->handle);
}

static int muxer_plugin_passthrough_submit_tags(void* ud, const taglist* tags, const segment_receiver* dest) {
    (void)ud;
    (void)tags;
    (void)dest;

    return 0;
}

static int muxer_plugin_passthrough_init(void) {
    return 0;
}

static void muxer_plugin_passthrough_deinit(void) {
    return;
}

static int muxer_plugin_passthrough_submit_dsi(void* ud, const membuf* data,const segment_receiver* dest) {
    (void)ud;
    (void)data;
    (void)dest;
    return 0;
}

static int muxer_plugin_passthrough_config(void* ud, const strbuf* key, const strbuf* val) {
    (void)ud;
    (void)key;
    (void)val;
    return 0;
}

static int muxer_plugin_passthrough_get_caps(void* ud, packet_receiver_caps* caps) {
    (void)ud;
    caps->has_global_header = 0;
    return 0;
}

const muxer_plugin muxer_plugin_passthrough = {
    {.a = 0, .len = 11, .x = (uint8_t*)"passthrough" },
    muxer_plugin_passthrough_init,
    muxer_plugin_passthrough_deinit,
    muxer_plugin_passthrough_create,
    muxer_plugin_passthrough_config,
    muxer_plugin_passthrough_open,
    muxer_plugin_passthrough_close,
    muxer_plugin_passthrough_submit_dsi,
    muxer_plugin_passthrough_submit_packet,
    muxer_plugin_passthrough_submit_tags,
    muxer_plugin_passthrough_flush,
    muxer_plugin_passthrough_get_caps,
};

