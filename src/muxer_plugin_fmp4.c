#include "muxer_plugin_fmp4.h"
#include "muxer_caps.h"

#include "minifmp4.h"
#include "bitwriter.h"

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "map.h"
#include "id3.h"

#include "pack_u32be.h"
#include "pack_u16be.h"
#include "unpack_u32le.h"
#include "unpack_u16le.h"

#define LOG_PREFIX "[muxer:fmp4]"
#include "logger.h"

static STRBUF_CONST(plugin_name,"fmp4");

static const char* AOID3_SCHEME_ID_URI = "https://aomedia.org/emsg/ID3";
static const char* AOID3_VALUE = "0";

static map measurement_method_keys;
static map measurement_system_keys;
static map reliability_keys;

#define CONFIGURING_MAIN 0
#define CONFIGURING_LOUDNESS 1
#define CONFIGURING_MEASUREMENT 2

static STRBUF_CONST(mime_mp4,"audio/mp4");
static STRBUF_CONST(mime_m4s,"audio/mp4");
static STRBUF_CONST(ext_mp4,".mp4");
static STRBUF_CONST(ext_m4s,".m4s");

#define KEY(v,t) static STRBUF_CONST(KEY_##v,#t)

KEY(loudness,loudness);
KEY(track,track);
KEY(album,album);
KEY(true_peak,true-peak);
KEY(sample_peak,sample-peak);
KEY(measurement_system,measurement-system);
KEY(reliability,reliability);
KEY(measurement_method,measurement-method);
KEY(value,value);

struct plugin_userdata {
    fmp4_mux mux;
    fmp4_track* track;
    fmp4_loudness* loudness;
    fmp4_measurement* measurement;
    fmp4_emsg* emsg;
    fmp4_sample_info default_info;
    membuf dsi;
    membuf expired_emsgs;
    unsigned int samples_per_segment;
    unsigned int segment_length;
    uint8_t configuring;
    id3 id3;
};

typedef struct plugin_userdata plugin_userdata;

static size_t plugin_size(void) {
    return sizeof(plugin_userdata);
}

static int plugin_create(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*) ud;

    fmp4_mux_init(&userdata->mux, NULL);

    id3_init(&userdata->id3);
    if(id3_ready(&userdata->id3) != 0) {
        return -1;
    }

    if( (userdata->track = fmp4_mux_new_track(&userdata->mux)) == NULL) {
        return -1;
    }

    if(fmp4_mux_add_brand(&userdata->mux,"aid3") != FMP4_OK) {
        return -1;
    }

    userdata->segment_length = 0;
    userdata->loudness = NULL;
    userdata->measurement = NULL;
    userdata->configuring = CONFIGURING_MAIN;
    userdata->emsg = NULL;

    membuf_init(&userdata->expired_emsgs);
    membuf_init(&userdata->dsi);

    return 0;
}

static int plugin_reset(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*) ud;

    if(userdata->emsg != NULL) {
        fmp4_emsg_free(userdata->emsg);
        userdata->emsg = NULL;
    }
    id3_reset(&userdata->id3);
    membuf_reset(&userdata->dsi);
    fmp4_track_set_base_media_decode_time(userdata->track, 0);
    userdata->track->dsi.len = 0;

    if(id3_ready(&userdata->id3) != 0) return -1;

    return 0;
}

static void expire_emsgs(plugin_userdata* userdata) {
    size_t i = 0;
    size_t len = userdata->expired_emsgs.len / sizeof(fmp4_emsg*);
    fmp4_emsg** e = (fmp4_emsg**)userdata->expired_emsgs.x;
    for(i=0;i<len;i++) {
        fmp4_emsg_free(e[i]);
    }
    membuf_reset(&userdata->expired_emsgs);
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(userdata->emsg != NULL) fmp4_emsg_free(userdata->emsg);
    fmp4_mux_close(&userdata->mux);
    expire_emsgs(userdata);
    membuf_free(&userdata->expired_emsgs);
    membuf_free(&userdata->dsi);
    id3_free(&userdata->id3);
}

static int plugin_config_measurement(plugin_userdata* userdata, const strbuf* key, const strbuf* value) {
    fmp4_result r;
    map_entry *e = NULL;

    if(strbuf_equals(key,&KEY_value)) {
        errno = 0;
        r = fmp4_measurement_set_value(userdata->measurement,strbuf_strtod(value));
        if(r != FMP4_OK || errno) {
            errno = 0;
            log_error("error parsing measurement value %.*s",(int)value->len,(char*)value->x);
            return -1;
        }
        return 0;
    }

    if(strbuf_equals(key,&KEY_measurement_system)) {
        e = map_find_lc(&measurement_system_keys,value);
        if(e != NULL) {
            r = fmp4_measurement_set_system(userdata->measurement,e->value.u8);
        } else {
            errno = 0;
            r = fmp4_measurement_set_system(userdata->measurement,strbuf_strtoul(value,10));
        }
        if(r != FMP4_OK || errno) {
            errno = 0;
            log_error("error parsing measurement-system value %.*s",(int)value->len,(char*)value->x);
            return -1;
        }
        return 0;
    }

    if(strbuf_equals(key,&KEY_reliability)) {
        /* see if we're using a string */
        e = map_find_lc(&reliability_keys,value);
        if(e != NULL) {
            r = fmp4_measurement_set_reliability(userdata->measurement,e->value.u8);
        } else {
            errno = 0;
            r = fmp4_measurement_set_reliability(userdata->measurement,strbuf_strtoul(value,10));
        }
        if(r != FMP4_OK || errno) {
            errno = 0;
            log_error("error parsing reliability value %.*s",(int)value->len,(char*)value->x);
            return -1;
        }
        return 0;
    }

    return -2;
}

static int plugin_config_loudness(plugin_userdata* userdata, const strbuf* key, const strbuf* value) {
    fmp4_result r;
    map_entry *e = NULL;

    if(strbuf_equals(key,&KEY_true_peak)) {
        errno = 0;
        r = fmp4_loudness_set_true_peak(userdata->loudness,strbuf_strtod(value));
        if(r != FMP4_OK || errno) {
            errno = 0;
            log_error("error parsing true-peak value %.*s",(int)value->len,(char*)value->x);
            return -1;
        }
        return 0;
    }

    if(strbuf_equals(key,&KEY_sample_peak)) {
        errno = 0;
        r = fmp4_loudness_set_sample_peak(userdata->loudness,strbuf_strtod(value));
        if(r != FMP4_OK || errno) {
            errno = 0;
            log_error("error parsing sample-peak value %.*s",(int)value->len,(char*)value->x);
            return -1;
        }
        return 0;
    }

    if(strbuf_equals(key,&KEY_measurement_system)) {
        e = map_find_lc(&measurement_system_keys,value);
        if(e != NULL) {
            r = fmp4_loudness_set_system(userdata->loudness,e->value.u8);
        } else {
            errno = 0;
            r = fmp4_loudness_set_system(userdata->loudness,strbuf_strtoul(value,10));
        }
        if(r != FMP4_OK || errno) {
            errno = 0;
            log_error("error parsing measurement-system value %.*s",(int)value->len,(char*)value->x);
            return -1;
        }
        return 0;
    }

    if(strbuf_equals(key,&KEY_reliability)) {
        e = map_find_lc(&reliability_keys,value);
        if(e != NULL) {
            r = fmp4_loudness_set_reliability(userdata->loudness,e->value.u8);
        } else {
            errno = 0;
            r = fmp4_loudness_set_reliability(userdata->loudness,strbuf_strtoul(value,10));
        }
        if(r != FMP4_OK || errno) {
            errno = 0;
            log_error("error parsing reliability value %.*s",(int)value->len,(char*)value->x);
            return -1;
        }
        return 0;
    }

    if(strbuf_equals(key,&KEY_measurement_method)) {
        if( (userdata->measurement = fmp4_loudness_new_measurement(userdata->loudness)) == NULL) {
            logs_error("error adding measurement to loudness");
            return -1;
        }

        e = map_find_lc(&measurement_method_keys,value);
        if(e != NULL) {
            r = fmp4_measurement_set_method(userdata->measurement,e->value.u8);
        } else {
            errno = 0;
            r = fmp4_measurement_set_method(userdata->measurement,strbuf_strtoul(value,10));
        }

        if(r != FMP4_OK || errno) {
            errno = 0;
            log_error("error parsing measurement value %.*s",(int)value->len,(char*)value->x);
            return -1;
        }
        userdata->configuring = CONFIGURING_MEASUREMENT;
        return 0;
    }

    return -2;
}

static int plugin_config_main(plugin_userdata* userdata, const strbuf* key, const strbuf* value) {
    if(strbuf_equals(key,&KEY_loudness)) {
        if( (userdata->loudness = fmp4_track_new_loudness(userdata->track)) == NULL) {
            logs_error("error allocating new loudness");
            return -1;
        }
        if(strbuf_equals(value,&KEY_track)) {
            userdata->loudness->type = FMP4_LOUDNESS_TRACK;
        } else if(strbuf_equals(value,&KEY_album)) {
            userdata->loudness->type = FMP4_LOUDNESS_ALBUM;
        } else {
            log_error("unknown loudness type %.*s",(int)value->len,(char *)value->x);
            return -1;
        }

        userdata->configuring = CONFIGURING_LOUDNESS;
        return 0;
    }

    return -2;
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* value) {
    int r = -2;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    switch(userdata->configuring) {
        case CONFIGURING_MAIN: r = plugin_config_main(userdata,key,value); break;
        case CONFIGURING_LOUDNESS: r = plugin_config_loudness(userdata,key,value); break;
        case CONFIGURING_MEASUREMENT: r = plugin_config_measurement(userdata,key,value); break;
        default: break;
    }

    if(r == -2) {
        log_error("unknown config key: %.*s",
          (int)key->len,(char *)key->x);
        r = -1;
    }

    return r;

}

struct segment_wrapper {
    const segment_receiver* dest;
    int samples;
    uint64_t pts;
};

typedef struct segment_wrapper segment_wrapper;

static size_t plugin_write_segment_callback(const void* src, size_t len, void* userdata) {
    const segment_wrapper* wrapper = (const segment_wrapper*)userdata;
    int r;
    segment s;

    s.type = SEGMENT_TYPE_MEDIA;
    s.data = src;
    s.len = len;
    s.samples = wrapper->samples;
    s.pts = wrapper->pts;

    r = wrapper->dest->submit_segment(wrapper->dest->handle,&s);
    return r == 0 ? len : 0;
}

static int plugin_submit_tags(void* ud, const taglist* tags, const segment_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    uint32_t id = 0;

    (void)dest; /* sendng tags is delayed until we write the segment */

    id3_reset(&userdata->id3);
    if(id3_add_taglist(&userdata->id3,tags) < 0) {
        fprintf(stderr,"had some kind of error on making a taglist!");
        return -1;
    }

    /* if we have an existing emsg,
     * set the event duration to the current sample count
     * and throw it in the expired list. */
    if(userdata->emsg != NULL) {
        userdata->emsg->event_duration = userdata->track->trun_sample_count;
        /* add this to the next segment write */
        if(fmp4_mux_add_emsg(&userdata->mux,userdata->emsg) != FMP4_OK) {
            return -1;
        }
        if(membuf_append(&userdata->expired_emsgs,&userdata->emsg,sizeof(fmp4_emsg*)) != 0) {
            return -1;
        }
        id = userdata->emsg->id + 1;
    }

    userdata->emsg = fmp4_emsg_new(NULL);
    if(userdata->emsg == NULL) return -1;

    userdata->emsg->version = 1;
    userdata->emsg->id = id;
    userdata->emsg->timescale = userdata->track->time_scale;
    if(fmp4_emsg_set_scheme_id_uri(userdata->emsg, AOID3_SCHEME_ID_URI) != FMP4_OK) return -1;
    if(fmp4_emsg_set_value(userdata->emsg, AOID3_VALUE) != FMP4_OK) return -1;
    userdata->emsg->presentation_time = userdata->track->base_media_decode_time + userdata->track->trun_sample_count;
    userdata->emsg->event_duration = 0xFFFFFFFF;
    if(fmp4_emsg_set_message(userdata->emsg,userdata->id3.x,userdata->id3.len) != FMP4_OK) return -1;

    return 0;
}

/* used in both the flush and submit packet functions. The flush function
 * just also calls the segment_handler's flush */
static int plugin_muxer_flush(plugin_userdata* userdata, const segment_receiver* dest) {
    fmp4_result res;
    segment_wrapper wrapper;

    wrapper.dest = dest;
    wrapper.samples = userdata->track->trun_sample_count;
    wrapper.pts = userdata->track->base_media_decode_time;

    if(userdata->emsg != NULL) {
        if(fmp4_mux_add_emsg(&userdata->mux,userdata->emsg) != FMP4_OK) return -1;
    }

    res = fmp4_mux_write_segment(&userdata->mux, plugin_write_segment_callback, &wrapper);
    if(res != FMP4_OK) return -1;

    if(userdata->emsg != NULL) {
        userdata->emsg->presentation_time = userdata->track->base_media_decode_time;
    }

    /* expire any old emsgs */
    expire_emsgs(userdata);

    return 0;
}

static int plugin_flush(void* ud, const segment_receiver* dest) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;
    if(userdata->track->trun_sample_count > 0) {
        if( (r = plugin_muxer_flush(userdata,dest)) != 0) return r;
    }
    return 0;
}

static int plugin_submit_packet(void* ud, const packet* packet, const segment_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;
    fmp4_sample_info info;

    fmp4_sample_info_init(&info);
    info.duration = packet->duration;
    info.size = packet->data.len;
    info.sample_group = packet->sample_group;
    info.flags.is_non_sync = !packet->sync;
    info.flags.depends_on = packet->sync ? 2 : 0;

    /* if our source has non-sync samples we should only flush when we get a sync packet */
    if(userdata->default_info.flags.is_non_sync) {
        if(packet->sync && userdata->track->trun_sample_count > 0) {
            if( (r = plugin_muxer_flush(userdata,dest)) != 0) return r;
        }
        if(fmp4_track_add_sample(userdata->track, packet->data.x, &info) != FMP4_OK) return -1;
        return 0;
    }

    /* default path where all samples are sync samples */
    /* if adding this sample would go over the total per segment, flush */
    /* see if we need to flush the current segment */
    if(userdata->track->trun_sample_count + packet->duration > userdata->samples_per_segment) {
        if( (r = plugin_muxer_flush(userdata,dest)) != 0) return r;
    }

    if(fmp4_track_add_sample(userdata->track, packet->data.x, &info) != FMP4_OK) return -1;

    return 0;
}

static size_t plugin_write_init_callback(const void* src, size_t len, void* userdata) {
    const segment_receiver* dest = (const segment_receiver*)userdata;
    segment s;

    s.type = SEGMENT_TYPE_INIT;
    s.data = src;
    s.len = len;
    s.pts = 0;

    return dest->submit_segment(dest->handle,&s) == 0 ? len : 0;
}

static int plugin_open(void* ud, const packet_source* source, const segment_receiver* dest) {
    int r;
    uint8_t buf[64];
    uint64_t tmp;
    size_t pos;
    size_t pos2;
    bitwriter bw;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    segment_source me = SEGMENT_SOURCE_ZERO;
    segment_source_info s_info = SEGMENT_SOURCE_INFO_ZERO;
    segment_params s_params = SEGMENT_PARAMS_ZERO;

    s_info.time_base = source->sample_rate;
    s_info.frame_len = source->frame_len;
    dest->get_segment_info(dest->handle,&s_info,&s_params);

    userdata->samples_per_segment = s_params.segment_length * s_info.time_base / 1000;

    userdata->track->stream_type = FMP4_STREAM_TYPE_AUDIO;

    me.handle         = userdata;
    me.init_ext       = &ext_mp4;
    me.media_ext      = &ext_m4s;
    me.init_mimetype  = &mime_mp4;
    me.media_mimetype = &mime_m4s;
    me.time_base      = source->sample_rate;
    me.frame_len      = source->frame_len;

    switch(source->codec) {
        case CODEC_TYPE_AAC: {
            userdata->track->codec = FMP4_CODEC_MP4A;
            userdata->track->object_type = FMP4_OBJECT_TYPE_AAC;
            break;
        }
        case CODEC_TYPE_ALAC: {
            userdata->track->codec = FMP4_CODEC_ALAC;
            break;
        }
        case CODEC_TYPE_FLAC: {
            userdata->track->codec = FMP4_CODEC_FLAC;
            break;
        }
        case CODEC_TYPE_OPUS: {
            userdata->track->codec = FMP4_CODEC_OPUS;
            break;
        }
        case CODEC_TYPE_MP3: {
            userdata->track->codec = FMP4_CODEC_MP4A;
            userdata->track->object_type = FMP4_OBJECT_TYPE_MP3;
            break;
        }
        case CODEC_TYPE_AC3: {
            userdata->track->codec = FMP4_CODEC_AC3;
            break;
        }
        case CODEC_TYPE_EAC3: {
            userdata->track->codec = FMP4_CODEC_EAC3;
            break;
        }
        default: {
            log_error("unsupported codec %s",codec_name(source->codec));
            return -1;
        }
    }


    fmp4_track_set_language(userdata->track,"und");
    userdata->track->time_scale = source->sample_rate;
    userdata->track->info.audio.channels = channel_count(source->channel_layout);
    fmp4_track_set_roll_distance(userdata->track,source->roll_distance);
    fmp4_track_set_encoder_delay(userdata->track,source->padding);
    if(source->roll_type == 1) {
        fmp4_track_set_roll_type(userdata->track, FMP4_ROLL_TYPE_PROL);
    }

    fmp4_sample_info_init(&userdata->default_info);
    userdata->default_info.duration = source->frame_len;
    userdata->default_info.flags.is_non_sync = source->sync_flag == 0;

    fmp4_track_set_default_sample_info(userdata->track, &userdata->default_info);

    if( (r = dest->open(dest->handle, &me)) != 0) {
        logs_error("error opening output");
        return r;
    }

    bitwriter_init(&bw);
    bw.buffer = buf;
    bw.len    = sizeof(buf);

    if(source->dsi.len > 0) {
        if(membuf_copy(&userdata->dsi,&source->dsi) != 0) {
            logs_fatal("error copying dsi");
            return -1;
        }
    }

    switch(userdata->track->codec) {
        case FMP4_CODEC_MP4A: {
            switch(userdata->track->object_type) {
                case FMP4_OBJECT_TYPE_AAC: {
                    if(userdata->dsi.len == 0) {
                        logs_fatal("expected dsi for AAC");
                        return -1;
                    }
                    break;
                }
                default: break;
            }
            break;
        }

        case FMP4_CODEC_OPUS: {
            /* the dsi we get is a whole OpusHead for Ogg, convert
             * to mp4 format */
            if(userdata->dsi.len <= 8 || memcmp(&userdata->dsi.x[0],"OpusHead",8) != 0) {
                logs_fatal("expected an OpusHead packet for dsi");
                return -1;
            }
            membuf_trim(&userdata->dsi,8);

            /* need to convert to mp4 format */

            /* change version from 1 to 0 */
            userdata->dsi.x[0] = 0x00;

            /* convert pre-skip, samplerate, and gain from little endian to big endian */
            pack_u16be(&userdata->dsi.x[2],unpack_u16le(&userdata->dsi.x[2]));
            pack_u32be(&userdata->dsi.x[4],unpack_u32le(&userdata->dsi.x[4]));
            pack_u16be(&userdata->dsi.x[8],unpack_u16le(&userdata->dsi.x[8]));
            break;
        }

        case FMP4_CODEC_ALAC: {
            /* the dsi we get has mp4box headers (4 bytes of size, 'alac', 4 bytes of flagss)
             * so we need to trim those */
            if(userdata->dsi.len <= 12) {
                logs_fatal("expected ALAC mp4box for dsi");
                return -1;
            }
            membuf_trim(&userdata->dsi,12);
            break;
        }

        case FMP4_CODEC_FLAC: {
            if(userdata->dsi.len != 34) {
                logs_fatal("expected FLAC STREAMINFO block for dsi");
                return -1;
            }
            pack_u32be(buf,0x80000000 | 34);
            if(membuf_insert(&userdata->dsi,buf,4,0) != 0) {
                return -1;
            }
            /* TODO check the channel layout and add a VORBISCOMMENT block if needed */
            switch(source->channel_layout) {
                case LAYOUT_MONO: /* fall-through */
                case LAYOUT_STEREO: /* fall-through */
                case LAYOUT_3_0: /* fall-through */
                case LAYOUT_QUAD: /* fall-through */
                case LAYOUT_5_0: /* fall-through */
                case LAYOUT_5_1: /* fall-through */
                case LAYOUT_6_1: /* fall-through */
                case LAYOUT_7_1: break;
                default: {
                    /* we need to add a metadata block to the dsi */
                    /* unset the last-metdata-block bit in current dsi */
                    userdata->dsi.x[0] &= 0x7F;

                    if(membuf_readyplus(&userdata->dsi,8) != 0) {
                        return -1;
                    }

                    /* save current position for writing header info */
                    pos = userdata->dsi.len;
                    userdata->dsi.len += 4;

                    /* save position for writing tag length */
                    pos2 = userdata->dsi.len;
                    userdata->dsi.len += 4;

                    if(source->name == NULL) {
                        strbuf_append_cstr(&userdata->dsi,"icecast-hls");
                    } else {
                        strbuf_cat(&userdata->dsi,source->name);
                    }
                    pack_u32be(buf,userdata->dsi.len - pos2 - 4);
                    memcpy(&userdata->dsi.x[pos2],buf,4);

                    pack_u32be(buf,1); /* only 1 tag */
                    if(membuf_append(&userdata->dsi,buf,4) != 0) return -1;

                    if(membuf_readyplus(&userdata->dsi,4) != 0) return -1;
                    pos2 = userdata->dsi.len;
                    userdata->dsi.len += 4;

                    if(strbuf_sprintf(&userdata->dsi,"WAVEFORMATEXTENSIBLE_CHANNEL_MASK=0x%llx",
                        source->channel_layout) != 0) return -1;
                    pack_u32be(buf,userdata->dsi.len - pos2 - 4);
                    memcpy(&userdata->dsi.x[pos2],buf,4);

                    /* finally write the header */
                    pack_u32be(buf, 0x84000000 | (userdata->dsi.len - pos - 4));
                    memcpy(&userdata->dsi.x[pos],buf,4);
                    break;
                }
            }
            break;
        }

        case FMP4_CODEC_AC3: {
            if(userdata->dsi.len == 0) {
                /* dsi is:
                   fscod, 2 bits
                   bsid, 5 bits
                   bsmod, 3 bits
                   acmod, 3 bits
                   lfeon, 1 bit
                   bit_rate_code, 5 bits
                   reserved, 5 bits */
                switch(source->sample_rate) {
                    case 48000: tmp = 0x00; break;
                    case 44100: tmp = 0x01; break;
                    case 32000: tmp = 0x02; break;
                    default: {
                        logs_fatal("unsupported sample rate for AC3");
                        return -1;
                    }
                }
                bitwriter_add(&bw, 2, tmp); /* fscod */
                bitwriter_add(&bw, 5, 8); /* bsid, hard-coded to 8 */
                bitwriter_add(&bw, 3, 0); /* bsmod, 0 = main service */
                switch(source->channel_layout & ~CHANNEL_LOW_FREQUENCY) {
                    case LAYOUT_MONO: tmp = 0x01; break;
                    case LAYOUT_STEREO: tmp = 0x02; break;
                    case LAYOUT_3_0: tmp = 0x03; break;
                    case LAYOUT_STEREO | CHANNEL_BACK_CENTER: tmp = 0x04; break;
                    case LAYOUT_4_0: tmp = 0x05; break;
                    case LAYOUT_QUAD: tmp = 0x06; break;
                    case LAYOUT_5_0: tmp = 0x07; break;
                    default: {
                        logs_fatal("unsupported channel layout");
                        return -1;
                    }
                }
                bitwriter_add(&bw, 3, tmp); /* acmod */
                bitwriter_add(&bw,1,!!(source->channel_layout & CHANNEL_LOW_FREQUENCY)); /* lfeon */
                tmp = 0;
                switch(source->bit_rate) {
                    case 640000: tmp++; /* fall-through */
                    case 576000: tmp++; /* fall-through */
                    case 512000: tmp++; /* fall-through */
                    case 448000: tmp++; /* fall-through */
                    case 384000: tmp++; /* fall-through */
                    case 320000: tmp++; /* fall-through */
                    case 256000: tmp++; /* fall-through */
                    case 224000: tmp++; /* fall-through */
                    case 192000: tmp++; /* fall-through */
                    case 160000: tmp++; /* fall-through */
                    case 128000: tmp++; /* fall-through */
                    case 112000: tmp++; /* fall-through */
                    case 96000:  tmp++; /* fall-through */
                    case 80000:  tmp++; /* fall-through */
                    case 64000:  tmp++; /* fall-through */
                    case 56000:  tmp++; /* fall-through */
                    case 48000:  tmp++; /* fall-through */
                    case 40000:  tmp++; /* fall-through */
                    case 32000:  break;
                    default: {
                        /* hard-code to 192 */
                        tmp = 0x0a;
                        break;
                    }
                }
                bitwriter_add(&bw,5,tmp); /* bit_rate_code */
                bitwriter_add(&bw,5,0x00); /* reserved, 5 bits */
                bitwriter_align(&bw);

                if(membuf_append(&userdata->dsi,buf,bw.len) != 0) {
                    logs_fatal("error copying dsi");
                    return -1;
                }
            }
            break;
        }

        case FMP4_CODEC_EAC3: {
            if(userdata->dsi.len == 0) {
                /* dsi is:
                   data_rate, 13 bits
                   num_ind_sub - 1, 3 bits
                   for each independent substream (only 1 for this app):
                       fscod, 2 bits
                       bsid, 5 bits
                       reserved, 1 bit
                       asvc, 1 bit
                       bsmod, 3 bits
                       acmod, 3 bits
                       lfeon, 1 bit
                       reserved, 3 bits
                       num_dep_Sub, 4 bits
                       if num_dep_sub > 0:
                           chan_loc, 9 bits
                       else:
                           reserved, 1 bit
                */
                if(source->bit_rate > 0) {
                    bitwriter_add(&bw, 13, source->bit_rate / 1000);
                } else {
                    bitwriter_add(&bw, 13, 192);
                }/* data_rate */

                bitwriter_add(&bw,  3,   0); /* num_ind_sub: TODO this is hard-coded to 0 */
                switch(source->sample_rate) {
                    case 48000: tmp = 0x00; break;
                    case 44100: tmp = 0x01; break;
                    case 32000: tmp = 0x02; break;
                    default: {
                        logs_fatal("unsupported sample rate for EAC3");
                        return -1;
                    }
                }
                bitwriter_add(&bw, 2, tmp); /* fscod */
                bitwriter_add(&bw, 5, 16); /* bsid, hard-coded to 16 */
                bitwriter_add(&bw, 1, 0); /* reserved 1 bit */
                bitwriter_add(&bw, 1, 0); /* asvc 1 bit, 0 = main service */
                bitwriter_add(&bw, 3, 0); /* bsmod, 0 = main service */
                switch(source->channel_layout & ~CHANNEL_LOW_FREQUENCY) {
                    case LAYOUT_MONO: tmp = 0x01; break;
                    case LAYOUT_STEREO: tmp = 0x02; break;
                    case LAYOUT_3_0: tmp = 0x03; break;
                    case LAYOUT_STEREO | CHANNEL_BACK_CENTER: tmp = 0x04; break;
                    case LAYOUT_4_0: tmp = 0x05; break;
                    case LAYOUT_QUAD: tmp = 0x06; break;
                    case LAYOUT_5_0: tmp = 0x07; break;
                    default: {
                        logs_fatal("unsupported channel layout");
                        return -1;
                    }
                }
                bitwriter_add(&bw, 3, tmp); /* acmod */
                bitwriter_add(&bw,1,!!(source->channel_layout & CHANNEL_LOW_FREQUENCY)); /* lfeon */
                bitwriter_add(&bw,3,0); /* 3 reserved bits */
                bitwriter_add(&bw,4,0); /* num_dep_sub: TODO This is hard-coded to 0 */
                bitwriter_add(&bw,1,0); /* reserved 1 bit */
                bitwriter_align(&bw);

                if(membuf_append(&userdata->dsi,buf,bw.len) != 0) {
                    logs_fatal("error copying dsi");
                    return -1;
                }
            }
            break;
        }

        default: break;
    }

    if(userdata->dsi.len > 0) {
        if( fmp4_track_set_dsi(userdata->track, userdata->dsi.x, userdata->dsi.len) != FMP4_OK) {
            logs_fatal("error setting dsi");
            return -1;
        }
    }

    return fmp4_mux_write_init(&userdata->mux, plugin_write_init_callback, (void *)dest) == FMP4_OK ? 0 : -1;
}

static uint32_t plugin_get_caps(void* ud) {
    (void)ud;
    return MUXER_CAP_GLOBAL_HEADERS;
}

static int plugin_init(void) {
    int r;
    map_init(&measurement_method_keys);
    map_init(&measurement_system_keys);
    map_init(&reliability_keys);

    /* measurement systems */
    if( (r = map_add_cstr_u8(&measurement_system_keys, "unknown",0)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "other",0)) != 0) return r;

    if( (r = map_add_cstr_u8(&measurement_system_keys, "ebu-r128",1)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "ebu r128",1)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "ebur128",1)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "r128",1)) != 0) return r;

    if( (r = map_add_cstr_u8(&measurement_system_keys, "itu-r bs.1770-3",2)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "itu bs.1770-3",2)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "bs.1770-3",2)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "bs.1770",2)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "bs1770",2)) != 0) return r;

    if( (r = map_add_cstr_u8(&measurement_system_keys, "itu-r bs.1770-3 pre",3)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "itu-r bs.1770-3pre",3)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "itu bs.1770-3 pre",3)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "itu bs.1770-3pre",3)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "bs.1770-3 pre",3)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "bs.1770-3pre",3)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "bs.1770 pre",3)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "bs.1770pre",3)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "bs1770 pre",3)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "bs1770pre",3)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "user",4)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "expert",5)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_system_keys, "panel",5)) != 0) return r;

    /* method definitions */
    if( (r = map_add_cstr_u8(&measurement_method_keys, "unknown",0)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "other",0)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "program loudness",1)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "program",1)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "anchor loudness",2)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "anchor",2)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "maximum of range",3)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "maximum of the range",3)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "maxrange",3)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "maximum momentary loudness",4)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "max momentary loudness",4)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "maximum momentary",4)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "max momentary",4)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "maximum short-term loudness",5)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "maximum short-term",5)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "max short-term loudness",5)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "max short-term",5)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "max short loudness",5)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "max short",5)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "loudness range",6)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "range",6)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "sound pressure level",7)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "sound pressure",7)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "pressure level",7)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "pressure",7)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "production room type index",8)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "production room index",8)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "room index",8)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "index",8)) != 0) return r;
    if( (r = map_add_cstr_u8(&measurement_method_keys, "room",8)) != 0) return r;

    /* reliability values */
    if( (r = map_add_cstr_u8(&reliability_keys, "unknown",0)) != 0) return r;
    if( (r = map_add_cstr_u8(&reliability_keys, "other",0)) != 0) return r;
    if( (r = map_add_cstr_u8(&reliability_keys, "unverified",1)) != 0) return r;
    if( (r = map_add_cstr_u8(&reliability_keys, "reported",1)) != 0) return r;
    if( (r = map_add_cstr_u8(&reliability_keys, "reported but unverified",1)) != 0) return r;
    if( (r = map_add_cstr_u8(&reliability_keys, "ceiling",2)) != 0) return r;
    if( (r = map_add_cstr_u8(&reliability_keys, "accurate",3)) != 0) return r;
    if( (r = map_add_cstr_u8(&reliability_keys, "measured",3)) != 0) return r;
    if( (r = map_add_cstr_u8(&reliability_keys, "measured and accurate",3)) != 0) return r;
    return 0;
}

static void plugin_deinit(void) {
    map_free(&measurement_system_keys);
    map_free(&measurement_method_keys);
    map_free(&reliability_keys);
    return;
}

static int plugin_get_segment_info(const void* ud, const packet_source_info* s, const segment_receiver* dest, packet_source_params* i) {
    (void)ud;

    segment_source_info s_info = SEGMENT_SOURCE_INFO_ZERO;
    segment_params s_params = SEGMENT_PARAMS_ZERO;

    s_info.time_base = s->time_base;
    s_info.frame_len = s->frame_len;

    dest->get_segment_info(dest->handle,&s_info,&s_params);

    i->segment_length = s_params.segment_length;
    i->packets_per_segment = s_params.packets_per_segment;

    return 0;
}

const muxer_plugin muxer_plugin_fmp4 = {
    plugin_name,
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
