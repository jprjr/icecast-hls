#include "muxer_plugin_packedaudio.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "id3.h"
#include "pack_u64be.h"

#define LOG0(str) fprintf(stderr,"[muxer:packed-audio] "str"\n")
#define LOG1(s, a) fprintf(stderr,"[muxer:packed-audio] "s"\n", (a))

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

static STRBUF_CONST(key_mpegts,"PRIV:com.apple.streaming.transportStreamTimestamp");

struct plugin_userdata {
    unsigned int segment_length;
    unsigned int packets_per_segment;
    unsigned int mpeg_samples_per_packet; /* packet # of samples scaled to mpeg-ts ticks */
    membuf samples;
    membuf segment;
    uint8_t profile;
    uint8_t freq;
    uint8_t ch_index;
    unsigned int packetcount;
    uint64_t ts; /* represents the 33-bit MPEG timestamp */
    id3 id3;
    taglist taglist;
    int (*append_packet)(struct plugin_userdata*, const packet*);
};
typedef struct plugin_userdata plugin_userdata;

static int append_packet_passthrough(struct plugin_userdata* userdata, const packet* p) {
    int r;
    if( (r = membuf_cat(&userdata->samples,&p->data)) != 0) {
        LOGERRNO("error appending packet");
    }
    return r;
}

static int append_packet_adts(struct plugin_userdata* userdata, const packet* p) {
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

    if( (r = membuf_append(&userdata->samples,adts_header,7)) != 0) {
        LOGERRNO("error appending packet");
    }

    if( (r = membuf_cat(&userdata->samples,&p->data)) != 0) {
        LOGERRNO("error appending packet");
    }
    return r;
}

static void* plugin_create(void) {
    plugin_userdata* userdata = NULL;
    TRYNULL(userdata = (plugin_userdata*)malloc(sizeof(plugin_userdata)),
      LOGERRNO("error allocating plugin"); abort());
    membuf_init(&userdata->samples);
    membuf_init(&userdata->segment);
    id3_init(&userdata->id3);
    taglist_init(&userdata->taglist);
    userdata->append_packet = NULL;
    userdata->profile = 0;
    userdata->freq = 0;
    userdata->ch_index = 0;
    userdata->packets_per_segment = 0;
    userdata->mpeg_samples_per_packet = 0;
    userdata->packetcount = 0;
    userdata->ts = 0x200000000ULL;

    return userdata;
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    membuf_free(&userdata->samples);
    membuf_free(&userdata->segment);
    id3_free(&userdata->id3);
    taglist_free(&userdata->taglist);
    free(userdata);
}

static int plugin_open(void* ud, const packet_source* source, const segment_receiver* dest) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    segment_source me = SEGMENT_SOURCE_ZERO;
    segment_source_info s_info = SEGMENT_SOURCE_INFO_ZERO;
    segment_params s_params = SEGMENT_PARAMS_ZERO;
    packet_source_params params = PACKET_SOURCE_PARAMS_ZERO;

    unsigned int sample_rate = source->sample_rate;
    unsigned int channels = source->channels;
    unsigned int profile = source->profile;

    if( (source->frame_len * 90000) % source->sample_rate != 0) {
        LOG1("WARNING, sample rate %u prevents MPEG-TS timestamps from aligning, consider resampling", source->sample_rate);
    }

    userdata->mpeg_samples_per_packet = source->frame_len * 90000 / source->sample_rate;

    s_info.time_base = 90000;
    s_info.frame_len = userdata->mpeg_samples_per_packet;
    dest->get_segment_params(dest->handle,&s_info, &s_params);
    params.packets_per_segment = s_params.packets_per_segment;
    me.handle = userdata;

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
            userdata->append_packet = append_packet_adts;
            userdata->profile = profile - 1;
            me.media_ext = &ext_aac;
            me.media_mimetype = &mime_aac;
            break;
        }
        case CODEC_TYPE_MP3: {
            userdata->append_packet = append_packet_passthrough;
            me.media_ext = &ext_mp3;
            me.media_mimetype = &mime_mp3;
            break;
        }

        case CODEC_TYPE_AC3: {
            userdata->append_packet = append_packet_passthrough;
            me.media_ext = &ext_ac3;
            me.media_mimetype = &mime_ac3;
            break;
        }

        case CODEC_TYPE_EAC3: {
            userdata->append_packet = append_packet_passthrough;
            me.media_ext = &ext_eac3;
            me.media_mimetype = &mime_eac3;
            break;
        }

        default: {
            LOG1("unsupported codec %s", codec_name(source->codec));
            return -1;
        }
    }

    if( (r = dest->open(dest->handle, &me)) != 0) return r;
    if( (r = id3_ready(&userdata->id3)) != 0) return r;

    userdata->ts -= (uint64_t)source->padding * (uint64_t)90000 / (uint64_t)source->sample_rate;

    return source->set_params(source->handle, &params);
}


static int plugin_send(plugin_userdata* userdata, const segment_receiver* dest) {
    segment s;
    tag ts_tag;
    uint8_t val_enc[8];
    int r;

    userdata->ts &= 0x1FFFFFFFFULL;
    pack_u64be(val_enc,userdata->ts);

    ts_tag.key = key_mpegts;
    ts_tag.value.x = val_enc;
    ts_tag.value.len = 8;
    ts_tag.value.a = 0;

    id3_reset(&userdata->id3);
    if( (r = id3_add_tag(&userdata->id3, &ts_tag)) != 0) {
        LOGERRNO("error adding tag");
        return r;
    }
    if( (r = membuf_cat(&userdata->segment, &userdata->id3)) != 0) {
        LOGERRNO("error concatenating segment");
        return r;
    }

    if(taglist_len(&userdata->taglist) > 0) {
        id3_reset(&userdata->id3);

        if( (r = id3_add_taglist(&userdata->id3,&userdata->taglist)) != 0) {
            LOGERRNO("error adding taglist");
            return r;
        }

        if( (r = membuf_cat(&userdata->segment, &userdata->id3)) != 0) {
            LOGERRNO("error concatenating segment");
            return r;
        }
    }

    if( (r = membuf_cat(&userdata->segment, &userdata->samples)) != 0) {
        LOGERRNO("error concatenating segment");
        return r;
    }

    s.type = SEGMENT_TYPE_MEDIA;
    s.data = userdata->segment.x;
    s.len  = userdata->segment.len;
    s.samples = userdata->packetcount * userdata->mpeg_samples_per_packet;
    s.pts = userdata->ts;

    if( (r = dest->submit_segment(dest->handle,&s)) != 0) {
        LOG0("error submitting segment");
        return r;
    }

    membuf_reset(&userdata->samples);
    membuf_reset(&userdata->segment);
    return r;
}

static int plugin_submit_packet(void* ud, const packet* packet, const segment_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;

    if( (r = userdata->append_packet(userdata,packet)) != 0) {
        return r;
    }
    userdata->packetcount++;

    if(userdata->packetcount >= userdata->packets_per_segment) {
        if( (r = plugin_send(userdata,dest)) != 0) return r;
        userdata->ts += (uint64_t)userdata->packetcount * (uint64_t)userdata->mpeg_samples_per_packet;
        userdata->packetcount = 0;
    }

    return r;
}

static int plugin_flush(void* ud, const segment_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;

    if(userdata->packetcount != 0) {
        if( (r = plugin_send(userdata,dest)) != 0) return r;
    }

    return dest->flush(dest->handle);
}

static int plugin_submit_tags(void* ud, const taglist* tags, const segment_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;
    (void)dest;

    if( (r = taglist_deep_copy(&userdata->taglist,tags)) != 0) { 
        LOGERRNO("error copying tags");
        return r;
    }

    return 0;
}

static int plugin_init(void) {
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static int plugin_submit_dsi(void* ud, const membuf* data,const segment_receiver* dest) {
    (void)ud;
    (void)data;
    (void)dest;
    return 0;
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* val) {
    (void)ud;
    (void)key;
    (void)val;
    return 0;
}

static uint32_t plugin_get_caps(void* ud) {
    (void)ud;
    return 0;
}

const muxer_plugin muxer_plugin_packed_audio = {
    {.a = 0, .len = 12, .x = (uint8_t*)"packed-audio" },
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_submit_dsi,
    plugin_submit_packet,
    plugin_submit_tags,
    plugin_flush,
    plugin_get_caps,
};

