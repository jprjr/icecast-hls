#include "muxer_plugin_ts.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <assert.h>

#include "id3.h"
#include "ts.h"
#include "adts_mux.h"
#include "pack_u16be.h"
#include "chunker.h"

#define LOG_PREFIX "[muxer:ts]"
#include "logger.h"

#define LOGERRNO(s) log_error(s": %s", strerror(errno))

static STRBUF_CONST(plugin_name,"ts");

static STRBUF_CONST(mime_ts,"video/mp2t");
static STRBUF_CONST(ext_ts, ".ts");

struct muxer_plugin_ts_userdata {
    membuf subsegment;
    membuf packet; /* contains the audio samples for the current logical packet */
    membuf dsi;
    membuf scratch;
    mpegts_stream audio_stream;
    mpegts_stream id3_stream;
    mpegts_header pat_header;
    mpegts_header pmt_header;

    codec_type codec;

    adts_mux adts_muxer;
    unsigned int padding;

    uint64_t segment_samplecount;
    uint64_t subsegment_samplecount;
    uint64_t packet_samplecount;
    uint64_t samples_per_segment;
    uint64_t samples_per_subsegment;
    uint64_t samples_per_packet;
    uint64_t subsegment_ts;
    uint64_t packet_ts;
    uint8_t newsegment;
    id3 id3;
    taglist taglist;
    chunker chunker;
    int (*submit_packet)(struct muxer_plugin_ts_userdata*, const packet*, const segment_receiver*);
};
typedef struct muxer_plugin_ts_userdata muxer_plugin_ts_userdata;

/* takes the buffered packet data and appends it to the segment */
static int muxer_plugin_ts_append_packet(muxer_plugin_ts_userdata* userdata) {
    int r;
    mpegts_pmt_params pmt_params;

    if(userdata->packet_samplecount == 0) return 0;
    if(userdata->packet.len == 0) return -1;

    pmt_params.codec = userdata->codec;
    pmt_params.audio_pid = 0x0100;
    pmt_params.id3_pid = 0x0101;
    pmt_params.dsi = &userdata->dsi;

    /* always send PAT and PMT when starting a new packet */
    if( (r = mpegts_header_encode(&userdata->subsegment, &userdata->pat_header)) != 0) return r;
    if( (r = mpegts_pat_encode(&userdata->subsegment, 0x1000)) != 0) return r;
    userdata->pat_header.cc = (userdata->pat_header.cc + 1) & 0x0f;

    if( (r = mpegts_header_encode(&userdata->subsegment, &userdata->pmt_header)) != 0) return r;
    if( (r = mpegts_pmt_encode(&userdata->subsegment, &pmt_params)) != 0) return r;
    userdata->pmt_header.cc = (userdata->pmt_header.cc + 1) & 0x0f;

    /* if this is a new, empty segment add any existing ID3 tags */
    if(userdata->newsegment) {
        userdata->newsegment = 0;
        if(taglist_len(&userdata->taglist) > 0) {
            id3_reset(&userdata->id3);
            if( (r = id3_add_taglist(&userdata->id3, &userdata->taglist)) != 0) return r;
            userdata->id3_stream.pts = userdata->audio_stream.pts;
            userdata->id3_stream.adaptation.pcr_flag = 0;
            if( (r = mpegts_stream_encode_packet(&userdata->subsegment, &userdata->id3_stream, &userdata->id3)) != 0) return r;
        }
    }

    /* always send PCR */
    userdata->audio_stream.adaptation.pcr_flag = 1;
    if( (r = mpegts_stream_encode_packet(&userdata->subsegment, &userdata->audio_stream, &userdata->packet)) != 0) return r;

    userdata->packet_ts += userdata->packet_samplecount;
    userdata->audio_stream.pts = rescale_duration(userdata->packet_ts, userdata->chunker.src_rate, 90000ULL) & 0x1FFFFFFFF;

    userdata->packet_samplecount = 0;
    userdata->packet.len = 0;
    return 0;
}


static int muxer_plugin_ts_subsegment_send(muxer_plugin_ts_userdata* userdata, const segment_receiver *dest, int reset) {
    int r;
    segment s = SEGMENT_ZERO;

    if(userdata->subsegment_samplecount == 0) return -1;
    if( (r = muxer_plugin_ts_append_packet(userdata)) != 0) return r;
    if(userdata->subsegment.len == 0) return -1;

    s.type = SEGMENT_TYPE_MEDIA;
    s.data = userdata->subsegment.x;
    s.len = userdata->subsegment.len;
    s.samples = userdata->subsegment_samplecount;
    s.pts = userdata->subsegment_ts;
    s.fin = reset;

    if( (r = dest->submit_segment(dest->handle, &s)) != 0) {
        logs_error("error submitting subsegment");
        return r;
    }
    membuf_reset(&userdata->subsegment);
    userdata->subsegment_ts       += userdata->subsegment_samplecount;
    userdata->subsegment_samplecount = 0;

    if(reset) {
        int syncsubseg = userdata->samples_per_segment == userdata->samples_per_subsegment;
        userdata->newsegment = 1;
        userdata->segment_samplecount = 0;
        userdata->samples_per_segment = chunker_next(&userdata->chunker);
        if(syncsubseg) userdata->samples_per_subsegment = userdata->samples_per_segment;
    }

    return 0;
}


static int muxer_plugin_submit_packet_passthrough(muxer_plugin_ts_userdata* userdata, const packet* packet, const segment_receiver *dest) {
    int r;

    if(userdata->segment_samplecount + ((uint64_t)packet->duration) > userdata->samples_per_segment) {
        if( (r = muxer_plugin_ts_subsegment_send(userdata,dest,1)) != 0) {
            return r;
        }
    } else {
        if(userdata->subsegment_samplecount + ((uint64_t)packet->duration) > userdata->samples_per_subsegment) {
            if( (r = muxer_plugin_ts_subsegment_send(userdata,dest,0)) != 0) {
            return r;
            }
        }
    }

    if((uint64_t)packet->duration + userdata->packet_samplecount > userdata->samples_per_packet) {
        if( (r = muxer_plugin_ts_append_packet(userdata)) != 0) return r;
    }

    if( (r = membuf_cat(&userdata->packet, &packet->data)) != 0) return r;
    userdata->packet_samplecount     += ((uint64_t)packet->duration);
    userdata->segment_samplecount    += ((uint64_t)packet->duration);
    userdata->subsegment_samplecount += ((uint64_t)packet->duration);

    if(userdata->segment_samplecount == userdata->samples_per_segment) {
        if( (r = muxer_plugin_ts_subsegment_send(userdata,dest,1)) != 0) {
            return r;
        }
    } else if(userdata->subsegment_samplecount == userdata->samples_per_subsegment) {
        if( (r = muxer_plugin_ts_subsegment_send(userdata,dest,0)) != 0) {
            return r;
        }
    }

    return 0;
}

static int muxer_plugin_submit_packet_adts(muxer_plugin_ts_userdata *ud, const packet* p, const segment_receiver *dest) {
    packet tmp = PACKET_ZERO;

    adts_mux_encode_packet(&ud->adts_muxer, p->data.x, p->data.len);
    tmp.duration     = p->duration;
    tmp.sample_rate  = p->sample_rate;
    tmp.sample_group = p->sample_group;
    tmp.pts          = p->pts;
    tmp.sync         = p->sync;

    tmp.data.x       = ud->adts_muxer.buffer;
    tmp.data.len     = ud->adts_muxer.len;

    return muxer_plugin_submit_packet_passthrough(ud, &tmp, dest);
}

static int muxer_plugin_submit_packet_opus_au(muxer_plugin_ts_userdata *ud, const packet* p, const segment_receiver *dest) {
    int r;
    packet tmp = PACKET_ZERO;
    size_t len = p->data.len;
    uint8_t u[2];

    membuf_reset(&ud->scratch);

    /* au header is 16 bits:
     *   11 bits 0x3ff = 01111111111
     *   1 bit start trim flag
     *   1 bit end trim flag
     *   1 bit control extension flag
     *   2 bits reserved
     */

    u[0] = 0x7f;
    u[1] = 0xe0;
    if(ud->padding) {
        u[1] |= 0x10;
    }

    if( (r = membuf_append(&ud->scratch, u, 2)) != 0) return r;

    /* then we encode the payload size */
    while(len >= 0xff) {
        if( (r = membuf_append(&ud->scratch,"\xff", 1)) != 0) return r;
        len -= 0xff;
    }
    u[0] = len;
    if( (r = membuf_append(&ud->scratch, u, 1)) != 0) return r;

    /* if we have padding info to encode do it on the first packet */
    if(ud->padding) {
        pack_u16be(u, (uint16_t) ud->padding);
        if( (r = membuf_append(&ud->scratch, u, 2)) != 0) return r;
        ud->padding = 0;
    }

    if( (r = membuf_cat(&ud->scratch, &p->data)) != 0) return r;

    tmp.duration     = p->duration;
    tmp.sample_rate  = p->sample_rate;
    tmp.sample_group = p->sample_group;
    tmp.pts          = p->pts;
    tmp.sync         = p->sync;

    tmp.data.x       = ud->scratch.x;
    tmp.data.len     = ud->scratch.len;

    return muxer_plugin_submit_packet_passthrough(ud, &tmp, dest);
}

static size_t muxer_plugin_ts_size(void) {
    return sizeof(muxer_plugin_ts_userdata);
}

static int muxer_plugin_ts_reset(void* ud) {
    muxer_plugin_ts_userdata* userdata = (muxer_plugin_ts_userdata*) ud;
    membuf_reset(&userdata->subsegment);
    membuf_reset(&userdata->packet);
    id3_reset(&userdata->id3);
    taglist_reset(&userdata->taglist);

    mpegts_header_init(&userdata->pat_header);
    mpegts_header_init(&userdata->pmt_header);
    mpegts_stream_init(&userdata->audio_stream);
    mpegts_stream_init(&userdata->id3_stream);

    userdata->pat_header.pid  = 0x0000;
    userdata->pat_header.pusi = 1;
    userdata->pat_header.adapt = 0x01;

    userdata->pmt_header.pid = 0x1000;
    userdata->pmt_header.pusi = 1;
    userdata->pmt_header.adapt = 0x01;

    userdata->audio_stream.header.pid = 0x0100;
    userdata->id3_stream.header.pid = 0x0101;
    userdata->id3_stream.stream_id = 0xBD;

    userdata->submit_packet = NULL;
    userdata->samples_per_segment = 0;
    userdata->samples_per_subsegment = 0;
    userdata->samples_per_packet = 0;
    userdata->subsegment_ts = 0;
    userdata->packet_ts = 0;
    userdata->segment_samplecount = 0;
    userdata->subsegment_samplecount = 0;
    userdata->packet_samplecount = 0;
    userdata->codec = CODEC_TYPE_UNKNOWN;
    userdata->padding = 0;
    userdata->newsegment = 1;
    userdata->chunker.i = 0;

    return 0;
}


static int muxer_plugin_ts_create(void* ud) {
    muxer_plugin_ts_userdata* userdata = (muxer_plugin_ts_userdata*) ud;

    membuf_init(&userdata->subsegment);
    membuf_init(&userdata->packet);
    membuf_init(&userdata->dsi);
    membuf_init(&userdata->scratch);
    id3_init(&userdata->id3);
    taglist_init(&userdata->taglist);

    return muxer_plugin_ts_reset(userdata);
}

static void muxer_plugin_ts_close(void* ud) {
    muxer_plugin_ts_userdata* userdata = (muxer_plugin_ts_userdata*)ud;

    membuf_free(&userdata->subsegment);
    membuf_free(&userdata->packet);
    membuf_free(&userdata->dsi);
    membuf_free(&userdata->scratch);
    id3_free(&userdata->id3);
    taglist_free(&userdata->taglist);
}

static int muxer_plugin_ts_open(void* ud, const packet_source* source, const segment_receiver* dest) {
    int r;
    muxer_plugin_ts_userdata* userdata = (muxer_plugin_ts_userdata*)ud;

    segment_source me = SEGMENT_SOURCE_ZERO;
    segment_source_info s_info = SEGMENT_SOURCE_INFO_ZERO;
    segment_params s_params = SEGMENT_PARAMS_ZERO;

    unsigned int sample_rate = source->sample_rate;
    uint64_t channel_layout = source->channel_layout;
    unsigned int profile = source->profile;

    if( (source->frame_len * 90000) % source->sample_rate != 0) {
        log_warn("sample rate %u prevents MPEG-TS timestamps from aligning, consider resampling", source->sample_rate);
    }

    s_info.time_base = source->sample_rate;
    s_info.frame_len = source->frame_len;
    dest->get_segment_info(dest->handle,&s_info, &s_params);

    userdata->chunker = chunker_create(source->sample_rate,
      rescale_duration(s_params.segment_length, 1000, source->sample_rate),
      source->frame_len);
    userdata->samples_per_segment = chunker_next(&userdata->chunker);
    userdata->samples_per_subsegment = s_params.subsegment_length
        ? rescale_duration(s_params.subsegment_length, 1000, s_info.time_base)
        : userdata->samples_per_segment;

    me.handle = userdata;
    me.time_base = source->sample_rate;
    me.frame_len = source->frame_len;
    me.sync_flag = 1;

    me.media_ext = &ext_ts;
    me.media_mimetype = &mime_ts;

    userdata->samples_per_packet  = rescale_duration(100,1000,source->sample_rate);

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
            userdata->audio_stream.stream_id = 0xC0;
            userdata->submit_packet =  muxer_plugin_submit_packet_adts;
            userdata->codec = source->codec;
            break;
        }

        case CODEC_TYPE_MP3: {
            userdata->audio_stream.stream_id = 0xC0;
            userdata->submit_packet =  muxer_plugin_submit_packet_passthrough;
            userdata->codec = source->codec;
            break;
        }

        case CODEC_TYPE_AC3: {
            userdata->audio_stream.stream_id = 0xBD;
            userdata->submit_packet =  muxer_plugin_submit_packet_passthrough;
            userdata->codec = source->codec;
            break;
        }

        case CODEC_TYPE_EAC3: {
            userdata->audio_stream.stream_id = 0xBD;
            userdata->submit_packet =  muxer_plugin_submit_packet_passthrough;
            userdata->codec = source->codec;
            break;
        }

        case CODEC_TYPE_OPUS: {
            userdata->audio_stream.stream_id = 0xBD;
            userdata->submit_packet =  muxer_plugin_submit_packet_opus_au;
            userdata->codec = source->codec;
            userdata->padding = source->padding;
            if( (r = membuf_copy(&userdata->dsi, &source->dsi)) != 0) return r;
            break;
        }

        default: {
            log_error("unsupported codec %s", codec_name(source->codec));
            return -1;
        }
    }

    if( (r = id3_ready(&userdata->id3)) != 0) return r;

    return dest->open(dest->handle, &me);
}

static int muxer_plugin_ts_submit_packet(void* ud, const packet* packet, const segment_receiver* dest) {
    muxer_plugin_ts_userdata* userdata = (muxer_plugin_ts_userdata*)ud;
    return userdata->submit_packet(userdata, packet, dest);
}

static int muxer_plugin_ts_flush(void* ud, const segment_receiver* dest) {
    muxer_plugin_ts_userdata* userdata = (muxer_plugin_ts_userdata*)ud;
    int r;

    if(userdata->subsegment_samplecount != 0) {
        if( (r = muxer_plugin_ts_subsegment_send(userdata, dest, 1)) != 0) return r;
    }

    userdata->segment_samplecount = 0;
    userdata->newsegment = 1;

    return 0;
}

static int muxer_plugin_ts_submit_tags(void* ud, const taglist* tags, const segment_receiver* dest) {
    muxer_plugin_ts_userdata* userdata = (muxer_plugin_ts_userdata*)ud;
    int r;
    (void)dest;

    if( (r = taglist_deep_copy(&userdata->taglist, tags)) != 0) {
        LOGERRNO("error copying tags");
        return r;
    }

    /* if we're mid-segment go ahead and send tags */
    if(userdata->subsegment.len > 0) {
        if(taglist_len(&userdata->taglist) > 0) {
            id3_reset(&userdata->id3);
            if( (r = id3_add_taglist(&userdata->id3, &userdata->taglist)) != 0) return r;
            userdata->id3_stream.pts = rescale_duration(userdata->packet_ts + ((uint64_t)userdata->packet_samplecount), userdata->chunker.src_rate, 90000ULL) & 0x1FFFFFFFF;
            userdata->id3_stream.adaptation.pcr_flag = 0;
            if( (r = mpegts_stream_encode_packet(&userdata->subsegment, &userdata->id3_stream, &userdata->id3)) != 0) return r;
        }
    }

    return 0;
}

static int muxer_plugin_ts_init(void) {
    return 0;
}

static void muxer_plugin_ts_deinit(void) {
    return;
}

static int muxer_plugin_ts_config(void* ud, const strbuf* key, const strbuf* val) {
    (void)ud;
    (void)key;
    (void)val;
    return 0;
}

static uint32_t muxer_plugin_ts_get_caps(void* ud) {
    (void)ud;
    return 0;
}

static int muxer_plugin_ts_get_segment_info(const void* ud, const packet_source_info* s, const segment_receiver* dest, packet_source_params* i) {
    (void)ud;

    segment_source_info s_info = SEGMENT_SOURCE_INFO_ZERO;
    segment_params s_params = SEGMENT_PARAMS_ZERO;

    s_info.time_base = s->time_base;
    s_info.frame_len = s->frame_len;

    dest->get_segment_info(dest->handle,&s_info,&s_params);

    i->segment_length = s_params.segment_length;
    i->packets_per_segment = s_params.packets_per_segment;
    i->segment_length = s_params.subsegment_length;
    i->packets_per_segment = s_params.packets_per_subsegment;

    return 0;
}

const muxer_plugin muxer_plugin_ts = {
    &plugin_name,
    muxer_plugin_ts_size,
    muxer_plugin_ts_init,
    muxer_plugin_ts_deinit,
    muxer_plugin_ts_create,
    muxer_plugin_ts_config,
    muxer_plugin_ts_open,
    muxer_plugin_ts_close,
    muxer_plugin_ts_submit_packet,
    muxer_plugin_ts_submit_tags,
    muxer_plugin_ts_flush,
    muxer_plugin_ts_reset,
    muxer_plugin_ts_get_caps,
    muxer_plugin_ts_get_segment_info,
};

