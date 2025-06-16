#include "muxer_plugin_packedaudio.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>

#include "id3.h"
#include "adts_mux.h"
#include "pack_u64be.h"

#define LOG_PREFIX "[muxer:packed-audio]"
#include "logger.h"

#define LOGERRNO(s) log_error(s": %s", strerror(errno))

static STRBUF_CONST(plugin_name,"packed-audio");

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
    uint64_t segment_samplecount;
    uint64_t subsegment_samplecount;
    uint64_t samples_per_segment;
    uint64_t samples_per_subsegment;
    membuf samples;
    membuf segment;
    adts_mux adts_muxer;
    uint64_t ts; /* represents the 33-bit MPEG timestamp */
    id3 id3;
    taglist taglist;
    int (*append_packet)(struct plugin_userdata*, const packet*);
    uint8_t newsegment;
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
    packet tmp = PACKET_ZERO;
    adts_mux_encode_packet(&userdata->adts_muxer, p->data.x, p->data.len);
    tmp.duration     = p->duration;
    tmp.sample_rate  = p->sample_rate;
    tmp.sample_group = p->sample_group;
    tmp.pts          = p->pts;
    tmp.sync         = p->sync;

    tmp.data.x       = userdata->adts_muxer.buffer;
    tmp.data.len     = userdata->adts_muxer.len;

    return append_packet_passthrough(userdata, &tmp);
}

static size_t plugin_size(void) {
    return sizeof(plugin_userdata);
}

static int plugin_reset(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*) ud;

    membuf_reset(&userdata->samples);
    membuf_reset(&userdata->segment);
    id3_reset(&userdata->id3);
    taglist_reset(&userdata->taglist);
    adts_mux_init(&userdata->adts_muxer);

    userdata->append_packet = NULL;
    userdata->samples_per_segment = 0;
    userdata->samples_per_subsegment = 0;
    userdata->ts = 0;
    userdata->segment_samplecount = 0;
    userdata->subsegment_samplecount = 0;
    userdata->newsegment = 1;

    return 0;
}

static int plugin_create(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*) ud;

    membuf_init(&userdata->samples);
    membuf_init(&userdata->segment);
    id3_init(&userdata->id3);
    taglist_init(&userdata->taglist);

    return plugin_reset(userdata);
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    membuf_free(&userdata->samples);
    membuf_free(&userdata->segment);
    id3_free(&userdata->id3);
    taglist_free(&userdata->taglist);
}

static int plugin_open(void* ud, const packet_source* source, const segment_receiver* dest) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    segment_source me = SEGMENT_SOURCE_ZERO;
    segment_source_info s_info = SEGMENT_SOURCE_INFO_ZERO;
    segment_params s_params = SEGMENT_PARAMS_ZERO;

    unsigned int sample_rate = source->sample_rate;
    uint64_t channel_layout = source->channel_layout;
    unsigned int profile = source->profile;

    if( (source->frame_len * 90000) % source->sample_rate != 0) {
        log_warn("sample rate %u prevents MPEG-TS timestamps from aligning, consider resampling", source->sample_rate);
    }

    s_info.time_base = 90000;
    if(source->frame_len != 0) {
        s_info.frame_len = source->frame_len * 90000 / source->sample_rate;
    }
    dest->get_segment_info(dest->handle,&s_info, &s_params);

    me.handle = userdata;
    me.time_base = 90000;
    me.frame_len = s_info.frame_len;
    me.sync_flag = 1;

    userdata->samples_per_segment = (uint64_t)s_params.segment_length * 90000ULL / 1000ULL;
    userdata->samples_per_subsegment = s_params.subsegment_length ? (uint64_t)s_params.subsegment_length * 90000ULL / 1000ULL : userdata->samples_per_segment;

    switch(source->codec) {
        case CODEC_TYPE_AAC: {
            adts_mux_init(&userdata->adts_muxer);

            switch(profile) {
                case CODEC_PROFILE_AAC_LC: break;
                case CODEC_PROFILE_AAC_HE2: {
                    if(source->channel_layout != LAYOUT_STEREO) {
                        log_error("unsupported channels for HE2: requires stereo, total channels=%u",
                          (unsigned int)channel_count(source->channel_layout));
                        return -1;
                    }
                    channel_layout = LAYOUT_MONO;
                }
                /* fall-through */
                case CODEC_PROFILE_AAC_HE: sample_rate /= 2; profile = CODEC_PROFILE_AAC_LC; break;
                case CODEC_PROFILE_AAC_USAC: /* fall-through */
                default: {
                    log_error("unsupported AAC profile %u",source->profile);
                    return -1;
                }
            }

            if(adts_mux_set_sample_rate(&userdata->adts_muxer, sample_rate) != 0) {
                log_error("unsupported sample rate %u", sample_rate);
                return -1;
            }

            if(adts_mux_set_channel_layout(&userdata->adts_muxer, channel_layout) != 0) {
                log_error("unsupported channel layout 0x%" PRIx64, channel_layout);
                return -1;
            }
            adts_mux_set_profile(&userdata->adts_muxer, profile);

            userdata->append_packet = append_packet_adts;
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
            log_error("unsupported codec %s", codec_name(source->codec));
            return -1;
        }
    }

    if( (r = dest->open(dest->handle, &me)) != 0) return r;
    if( (r = id3_ready(&userdata->id3)) != 0) return r;

    return 0;
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

    if(userdata->newsegment) {
        userdata->newsegment = 0;
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
    }

    if( (r = membuf_cat(&userdata->segment, &userdata->samples)) != 0) {
        LOGERRNO("error concatenating segment");
        return r;
    }

    s.type = SEGMENT_TYPE_MEDIA;
    s.data = userdata->segment.x;
    s.len  = userdata->segment.len;
    s.samples = userdata->subsegment_samplecount;
    s.pts = userdata->ts;
    s.independent = 1;

    if( (r = dest->submit_segment(dest->handle,&s)) != 0) {
        logs_error("error submitting segment");
        return r;
    }

    membuf_reset(&userdata->samples);
    membuf_reset(&userdata->segment);
    return r;
}

static int plugin_submit_packet(void* ud, const packet* packet, const segment_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;
    uint64_t rescaled_duration = ((uint64_t)packet->duration) * 90000ULL / ((uint64_t)packet->sample_rate);


    if(userdata->segment_samplecount + userdata->subsegment_samplecount + rescaled_duration > userdata->samples_per_segment) {
        if( (r = plugin_send(userdata,dest)) != 0) return r;
        userdata->ts += (uint64_t)userdata->subsegment_samplecount;
        userdata->segment_samplecount = 0;
        userdata->subsegment_samplecount = 0;
        userdata->newsegment = 1;
    } else {
        if(userdata->subsegment_samplecount + rescaled_duration > userdata->samples_per_subsegment) {
            if( (r = plugin_send(userdata,dest)) != 0) return r;
            userdata->ts += (uint64_t)userdata->subsegment_samplecount;
            userdata->segment_samplecount += userdata->subsegment_samplecount;
            userdata->subsegment_samplecount = 0;
        }
    }

    if( (r = userdata->append_packet(userdata,packet)) != 0) {
        return r;
    }
    userdata->subsegment_samplecount += rescaled_duration;

    return r;
}

static int plugin_flush(void* ud, const segment_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;

    if(userdata->subsegment_samplecount != 0) {
        if( (r = plugin_send(userdata,dest)) != 0) return r;
    }

    return 0;
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

static int plugin_get_segment_info(const void* ud, const packet_source_info* s, const segment_receiver* dest, packet_source_params* i) {
    (void)ud;

    segment_source_info s_info = SEGMENT_SOURCE_INFO_ZERO;
    segment_params s_params = SEGMENT_PARAMS_ZERO;

    s_info.time_base = 90000;
    s_info.frame_len = s->frame_len * 90000 / s->time_base;

    dest->get_segment_info(dest->handle,&s_info,&s_params);

    i->segment_length = s_params.segment_length;
    i->packets_per_segment = s_params.packets_per_segment;

    if(s_params.subsegment_length) {
        i->segment_length = s_params.subsegment_length;
    }

    if(s_params.packets_per_subsegment) {
        i->packets_per_segment = s_params.packets_per_subsegment;
    }

    return 0;
}

const muxer_plugin muxer_plugin_packed_audio = {
    &plugin_name,
    plugin_size,
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_submit_packet,
    plugin_submit_tags,
    plugin_flush,
    plugin_reset,
    plugin_get_caps,
    plugin_get_segment_info,
};

