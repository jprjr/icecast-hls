#include "demuxer_plugin_ogg.h"

#include "base64decode.h"
#include "unpack_u32be.h"
#include "unpack_u32le.h"
#include "unpack_u16be.h"
#include "unpack_u16le.h"

#define MINIOGG_API static
#include "miniogg.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_PAGES 10

#define LOG0(fmt) fprintf(stderr, "[demuxer:ogg] " fmt "\n")
#define LOG1(fmt,a) fprintf(stderr, "[demuxer:ogg] " fmt "\n", (a))
#define LOG2(fmt,a,b) fprintf(stderr, "[demuxer:ogg] " fmt "\n", (a), (b))
#define LOG4(fmt,a,b,c,d) fprintf(stderr, "[demuxer:ogg] " fmt "\n", (a), (b), (c), (d))

#define OPUS_DURATION_2_5MS 120
#define OPUS_DURATION_5MS   (OPUS_DURATION_2_5MS * 2)
#define OPUS_DURATION_10MS  (OPUS_DURATION_5MS   * 2)
#define OPUS_DURATION_20MS  (OPUS_DURATION_10MS  * 2)
#define OPUS_DURATION_40MS  (OPUS_DURATION_20MS  * 2)
#define OPUS_DURATION_60MS  (OPUS_DURATION_20MS  * 3)


/* 1-byte marker 0x7F
 * 4-byte signature FLAC
 * 1-byte major version (1)
 * 1-byte minor version (0)
 * 2-byte number of non-audio packets (ignored)
 * 4-byte fLaC marker
 * 4-byte STREAMINFO headers
 * 34-byte STREAMINFO block
 */
#define STREAMINFO_OFFSET 17
#define STREAMINFO_SIZE 34

static const unsigned int opus_frame_sizes[32] = {
    OPUS_DURATION_10MS, OPUS_DURATION_20MS, OPUS_DURATION_40MS, OPUS_DURATION_60MS,
    OPUS_DURATION_10MS, OPUS_DURATION_20MS, OPUS_DURATION_40MS, OPUS_DURATION_60MS,
    OPUS_DURATION_10MS, OPUS_DURATION_20MS, OPUS_DURATION_40MS, OPUS_DURATION_60MS,
    OPUS_DURATION_10MS, OPUS_DURATION_20MS,
    OPUS_DURATION_10MS, OPUS_DURATION_20MS,
    OPUS_DURATION_2_5MS, OPUS_DURATION_5MS, OPUS_DURATION_10MS, OPUS_DURATION_20MS,
    OPUS_DURATION_2_5MS, OPUS_DURATION_5MS, OPUS_DURATION_10MS, OPUS_DURATION_20MS,
    OPUS_DURATION_2_5MS, OPUS_DURATION_5MS, OPUS_DURATION_10MS, OPUS_DURATION_20MS,
    OPUS_DURATION_2_5MS, OPUS_DURATION_5MS, OPUS_DURATION_10MS, OPUS_DURATION_20MS,
};

enum OGG_TYPE {
    OGG_TYPE_UNKNOWN,
    OGG_TYPE_FLAC,
    OGG_TYPE_OPUS,
};

typedef enum OGG_TYPE OGG_TYPE;

static STRBUF_CONST(plugin_name, "ogg");

struct plugin_userdata {
    input* input;
    uint32_t serialno;
    membuf scratch;
    membuf buffer;
    miniogg ogg;
    packet packet;
    size_t bufpos;
    OGG_TYPE oggtype;
    taglist tags;
    uint8_t ignore_tags;
    uint8_t empty_tags;
    uint64_t granulepos;
    uint64_t granuleoffset;
    packet_source me;
};

typedef struct plugin_userdata plugin_userdata;

static size_t plugin_size(void) {
    return sizeof(plugin_userdata);
}

static size_t buffer_read(plugin_userdata* userdata, size_t len) {
    size_t r;
    int t;

    if( (t = membuf_readyplus(&userdata->buffer,len)) != 0) {
        LOG1("error allocating buffer %d",t);
        return 0;
    }
    r = input_read(userdata->input,&userdata->buffer.x[userdata->buffer.len],len);
    userdata->buffer.len += r;
    return r;
}

static unsigned int opus_get_duration(const uint8_t* packet, size_t packetlen) {
    unsigned int duration = 0;
    uint8_t framecode     = 0;

    if(packetlen < 1) return 0;

    /* peep the packet for a duration */
    duration  = (packet[0] >> 3) & 0x1F;
    framecode = (packet[0] & 0x03);

    duration = opus_frame_sizes[duration];
    switch(framecode) {
        case 1: /* fall-through */
        case 2: duration *= 2; break;
        case 3: {
            if(packetlen < 2) return 0;
            duration *= (unsigned int)( (packet[1] & 0x3F) );
            break;
        }
        default: break;
    }

    return duration;
}

static int loadpage(plugin_userdata* userdata) {
    int r = 0;
    size_t used = 0;
    size_t re = 0;
    size_t firstpacket = 0;
    size_t i = 0;
    const uint8_t* packet = NULL;
    size_t packetlen = 0;
    uint64_t granulepos = 0;
    uint8_t cont = 0;
    uint64_t offset = 0;

    while( (r = miniogg_add_page(&userdata->ogg, &userdata->buffer.x[userdata->bufpos],userdata->buffer.len - userdata->bufpos, &used)) == 1) {
        userdata->bufpos = 0;
        userdata->buffer.len = 0;
        if( (re = buffer_read(userdata,4096)) == 0) {
            break; /* will return 1 == end-of-file */
        }
    }
    if(r != 0) return r;

    if(userdata->granuleoffset == ~0ULL && userdata->ogg.granulepos != ~0ULL && userdata->ogg.granulepos > 0) {
        /* seeing our first granulepos, let's determine our offset based on decoded packets.
         * Basically, if we've been seeing this stream since the actual beginning, our internal pts
         * will sync with granulepos, but if we entered during the middle of the stream it'll be
         * larger than ours, so determine an offset to add to our own.
         *
         * This is basically just for Opus - on the final page, it can signal the last
         * packet only represents (x) samples of audio by setting a granulepos that
         * is lower than your "expected" granulepos. So we'll need the offset to
         * get the value correct. */

        /* set this to 0 to avoid checking again */
        userdata->granuleoffset = 0;

        /* if the continuation flag is sent, that means we haven't demuxed the packet
         * anyway and don't need to worry about checking its duration */
        if(userdata->ogg.continuation) firstpacket = 1;

        i = userdata->ogg.packets;
        offset = 0;
        /* work backwards through packets */
        while(i-- > firstpacket) {
            packet = miniogg_get_packet(&userdata->ogg, (uint32_t)i, &packetlen, &granulepos, &cont);
            offset += (uint64_t)opus_get_duration(packet,packetlen);
        }
        if(offset <= userdata->ogg.granulepos) {
            userdata->granuleoffset = userdata->ogg.granulepos - offset;
        }
    }

    userdata->bufpos += used;
    return 0;
}

static int getpacket(plugin_userdata* userdata) {
    int r;

    const uint8_t *data;
    size_t datalen;
    uint8_t cont = 1;

    membuf_reset(&userdata->packet.data);

    while(cont) {
        while( (data = miniogg_iter_packet(&userdata->ogg, &datalen, &userdata->granulepos, &cont)) == NULL) {
            /* no packets left - if this was the last page flush the receiver to
             * prep for new packets */
            if(userdata->ogg.eos != 0) {
                userdata->oggtype = OGG_TYPE_UNKNOWN;
                taglist_reset(&userdata->tags);
                userdata->granuleoffset = ~0ULL;
                return 2; /* signal end of stream */
            }

            if( (r = loadpage(userdata)) != 0) return r;

            /* if this page is for another stream, try again */
            while(userdata->ogg.serialno != userdata->serialno) {
                if( (r = loadpage(userdata)) != 0) return r;
            }
        }

        if( (r = membuf_append(&userdata->packet.data, data, datalen)) != 0) {
            LOG0("error appending packet to buffer");
            return r;
        }
    }

    return 0;
}

static int plugin_init(void) {
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static int plugin_create(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    miniogg_init(&userdata->ogg,0);

    userdata->bufpos = 0;
    userdata->oggtype = OGG_TYPE_UNKNOWN;

    membuf_init(&userdata->buffer);
    membuf_init(&userdata->scratch);
    packet_init(&userdata->packet);
    taglist_init(&userdata->tags);

    userdata->ignore_tags = 0;
    userdata->empty_tags = 0;
    userdata->granuleoffset = ~0ULL;
    userdata->me = packet_source_zero;

    return 0;
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    membuf_free(&userdata->buffer);
    membuf_free(&userdata->scratch);
    packet_free(&userdata->packet);
    taglist_free(&userdata->tags);
    membuf_free(&userdata->me.dsi);
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* value) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(strbuf_equals_cstr(key,"empty tags") || strbuf_equals_cstr(key,"empty-tags")) {
        if(strbuf_truthy(value)) {
            userdata->empty_tags = 1;
            return 0;
        }
        if(strbuf_falsey(value)) {
            userdata->empty_tags = 0;
            return 0;
        }
        if(strbuf_caseequals_cstr(value,"keep")) {
            userdata->empty_tags = 1;
            return 0;
        }
        if(strbuf_caseequals_cstr(value,"remove")) {
            userdata->empty_tags = 0;
            return 0;
        }
        LOG4("unknown value for key %.*s: %.*s",
          (int)key->len,(char *)key->x,(int)value->len,(char *)value->x);

        return -1;
    }

    if(strbuf_equals_cstr(key,"ignore tags") || strbuf_equals_cstr(key,"ignore-tags")) {
        if(strbuf_truthy(value)) {
            userdata->ignore_tags = 1;
            return 0;
        }
        if(strbuf_falsey(value)) {
            userdata->ignore_tags = 0;
            return 0;
        }
        LOG4("unknown value for key %.*s: %.*s",
          (int)key->len,(char *)key->x,(int)value->len,(char *)value->x);
        return -1;
    }

    LOG2("unknown key %.*s",
      (int)key->len,(char *)key->x);
    return -1;
}

static int plugin_open(void* ud, input* in) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    userdata->input = in;

    if( buffer_read(userdata, 4) != 4) {
        return -1;
    }

    if(userdata->buffer.x[0] != 'O' ||
       userdata->buffer.x[1] != 'g' ||
       userdata->buffer.x[2] != 'g' ||
       userdata->buffer.x[3] != 'S') {
        LOG0("missing OggS signature");
        return -1;
    }

    return 0;
}

static int handle_comment_block(plugin_userdata* userdata) {
    int r;
    size_t i = 0;
    size_t c = 0;
    uint32_t comments = 0;

    strbuf key = STRBUF_ZERO;
    strbuf val = STRBUF_ZERO;
    strbuf eq  = STRBUF_ZERO;

    /* ignore the vendor string */
    comments = unpack_u32le(&userdata->packet.data.x[i]);
    if(comments == 0) return 0;
    i += 4 + comments;
    if(i >= userdata->packet.data.len) return 0;
    /* end vendor string */

    comments = unpack_u32le(&userdata->packet.data.x[i]);
    i += 4;

    for(c=0;c<comments;c++) {
        if(i == userdata->packet.data.len) break;

        key.len = unpack_u32le(&userdata->packet.data.x[i]);
        i += 4;
        if(i > userdata->packet.data.len) break;

        key.x = &userdata->packet.data.x[i];
        i += key.len;
        if(i > userdata->packet.data.len) break;

        if(strbuf_chrbuf(&eq,&key,'=') != 0) continue;
        val.x = &eq.x[1];
        val.len = eq.len - 1;
        key.len -= val.len + 1;

        if(key.len == 0) continue;
        if(val.len == 0 && !userdata->empty_tags) continue;
        strbuf_lower(&key);

        if(strbuf_equals_cstr(&key,"metadata_block_picture")) {
            /* base64-decode the picture block */
            if( (r = membuf_ready(&userdata->scratch,val.len) != 0)) {
                LOG0("failed to allocate image buffer");
                return r;
            }
            userdata->scratch.len = val.len;
            if( (r = base64decode(val.x,val.len,userdata->scratch.x,&userdata->scratch.len)) != 0) {
                LOG1("base64 decode failed: %d",r);
                return r;
            }
            val.x = userdata->scratch.x;
            val.len = userdata->scratch.len;
        } else if(strbuf_equals_cstr(&key,"waveformatextensible_channel_mask")) {
            userdata->me.channel_layout = strbuf_strtoull(&val, 16);
            continue;
        }

        if(!userdata->ignore_tags) {
            if( (r = taglist_add(&userdata->tags,&key,&val)) != 0) return r;
        }
    }

    return 0;
}

static int handle_flac_comment_block(plugin_userdata* userdata) {
    int r;
    uint8_t *x = userdata->packet.data.x;
    userdata->packet.data.x += 4;
    userdata->packet.data.len -= 4;
    r = handle_comment_block(userdata);
    userdata->packet.data.x = x;
    return r;
}

static int handle_flac_picture_block(plugin_userdata* userdata) {
    strbuf key = STRBUF_ZERO;
    strbuf val = STRBUF_ZERO;

    if(userdata->packet.data.len <= 4) return 0;

    key.x = (uint8_t*)"metadata_block_picture";
    key.len = strlen("metadata_block_picture");

    val.x = &userdata->buffer.x[4];
    val.len = userdata->packet.data.len - 4;

    return taglist_add(&userdata->tags,&key,&val);
}

static int handle_opus_comment_block(plugin_userdata* userdata) {
    int r;
    uint8_t *x = userdata->packet.data.x;
    userdata->packet.data.x += 8;
    userdata->packet.data.len -= 8;
    r = handle_comment_block(userdata);
    userdata->packet.data.x = x;
    return r;
}

static int plugin_run_flac(plugin_userdata* userdata, const tag_handler* thandler, const packet_receiver* receiver) {
    int r;
    uint8_t bos = 0; /* if we see a bos flag we need to go through all header-type
                        packets, since vorbis comments can change channel layout */
    uint16_t min_block_size = 0;
    uint16_t max_block_size = 0;
    uint8_t channels = 0;
    size_t i = 0;

    if( (r = getpacket(userdata)) != 0) return r;

    if(userdata->ogg.bos != 0) {
        bos = 1;
    }

    while(bos) {
        if(userdata->packet.data.x[0] == 0x7F) {
            /* STREAMINFO packet */
            if(userdata->packet.data.len < (STREAMINFO_SIZE + STREAMINFO_OFFSET) ||
               memcmp(userdata->packet.data.x,"\x7F""FLAC""\x01""\x00",7) != 0) {
                /* malformed stream, bail */
                return -1;
            }
            packet_source_reset(&userdata->me);
            if( (r = membuf_append(&userdata->me.dsi,&userdata->packet.data.x[STREAMINFO_OFFSET],STREAMINFO_SIZE)) != 0) return r;

            min_block_size = unpack_u16be(&userdata->me.dsi.x[0]);
            max_block_size = unpack_u16be(&userdata->me.dsi.x[2]);
            channels       = ((userdata->me.dsi.x[12] >> 1) & 0x07) + 1;

            if(min_block_size == max_block_size) userdata->me.frame_len = min_block_size;

            userdata->me.name = &plugin_name;
            userdata->me.handle = userdata;
            userdata->me.codec = CODEC_TYPE_FLAC;
            userdata->me.sync_flag = 1;
            userdata->me.sample_rate = unpack_u32be(&userdata->me.dsi.x[10]) >> 12;

            switch(channels) {
                case 1: userdata->me.channel_layout = LAYOUT_MONO; break;
                case 2: userdata->me.channel_layout = LAYOUT_STEREO; break;
                case 3: userdata->me.channel_layout = LAYOUT_3_0; break;
                case 4: userdata->me.channel_layout = LAYOUT_QUAD; break;
                case 5: userdata->me.channel_layout = LAYOUT_5_0; break;
                case 6: userdata->me.channel_layout = LAYOUT_5_1; break;
                case 7: userdata->me.channel_layout = LAYOUT_6_1; break;
                case 8: userdata->me.channel_layout = LAYOUT_7_1; break;
            }
            /* the channel_layout can be updated via tag */

            userdata->packet.sample_rate = userdata->me.sample_rate;
            userdata->packet.sync = 1;
            userdata->packet.pts = 0;

        } else if(userdata->packet.data.x[0] == 0xFF) {
            /* frame header, move on to decoding */
            bos = 0;
            if( (r = receiver->open(receiver->handle, &userdata->me)) != 0) return r;
            if( taglist_len(&userdata->tags) > 0) {
                if( (r = thandler->cb(thandler->userdata,&userdata->tags)) != 0) return r;
            }
            break;
        } else {
            switch(userdata->packet.data.x[0] & 0x7F) {
                case 0: {
                    /* a 2nd streaminfo? */
                    return -1;
                }
                case 4: {
                    /* handle vorbis comments */
                    if( (r = handle_flac_comment_block(userdata)) != 0) {
                        return r;
                    }
                    break;
                }
                case 6: {
                    /* handle an embedded picture */
                    if( (r = handle_flac_picture_block(userdata)) != 0) {
                        return r;
                    }
                    break;
                }
                default: break;
            }
        }
        if( (r = getpacket(userdata)) != 0) return r;
    }

    /* make sure we have an audio frame */
    if(userdata->packet.data.x[0] != 0xFF) {
        return -1;
    }

    /* get the duration */
    userdata->packet.duration = (userdata->packet.data.x[2] >> 4) & 0x0F;
    switch(userdata->packet.duration) {
        case 0: return -1;
        case 1: {
            userdata->packet.duration = 192;
            break;
        }
        case 2: {
            userdata->packet.duration = 576;
            break;
        }
        case 3: {
            userdata->packet.duration = 1152;
            break;
        }
        case 4: {
            userdata->packet.duration = 2304;
            break;
        }
        case 5: {
            userdata->packet.duration = 4608;
            break;
        }
        case 6: {
            i = 4;
            if( (userdata->packet.data.x[4] & 0x80) == 0x00) {
                i += 1;
            } else if( (userdata->packet.data.x[4] & 0xE0) == 0xC0) {
                i += 2;
            } else if( (userdata->packet.data.x[4] & 0xF0) == 0xE0) {
                i += 3;
            } else if( (userdata->packet.data.x[4] & 0xF8) == 0xF0) {
                i += 4;
            } else if( (userdata->packet.data.x[4] & 0xFC) == 0xF8) {
                i += 5;
            } else if( (userdata->packet.data.x[4] & 0xFE) == 0xFC) {
                i += 6;
            } else if( (userdata->packet.data.x[4] & 0xFF) == 0xFE) {
                i += 7;
            }
            userdata->packet.duration = userdata->packet.data.x[i] + 1;
            break;
        }
        case 7: {
            i = 4;
            if( (userdata->packet.data.x[4] & 0x80) == 0x00) {
                i += 1;
            } else if( (userdata->packet.data.x[4] & 0xE0) == 0xC0) {
                i += 2;
            } else if( (userdata->packet.data.x[4] & 0xF0) == 0xE0) {
                i += 3;
            } else if( (userdata->packet.data.x[4] & 0xF8) == 0xF0) {
                i += 4;
            } else if( (userdata->packet.data.x[4] & 0xFC) == 0xF8) {
                i += 5;
            } else if( (userdata->packet.data.x[4] & 0xFE) == 0xFC) {
                i += 6;
            } else if( (userdata->packet.data.x[4] & 0xFF) == 0xFE) {
                i += 7;
            }
            userdata->packet.duration = unpack_u16be(&userdata->packet.data.x[i]) + 1;
            break;
        }
        case 8: {
            userdata->packet.duration = 256;
            break;
        }
        case 9: {
            userdata->packet.duration = 512;
            break;
        }
        case 10: {
            userdata->packet.duration = 1024;
            break;
        }
        case 11: {
            userdata->packet.duration = 2048;
            break;
        }
        case 12: {
            userdata->packet.duration = 4096;
            break;
        }
        case 13: {
            userdata->packet.duration = 8192;
            break;
        }
        case 14: {
            userdata->packet.duration = 16384;
            break;
        }
        case 15: {
            userdata->packet.duration = 32768;
            break;
        }
        default: break;
    }

    r = receiver->submit_packet(receiver->handle, &userdata->packet);

    userdata->packet.pts += (uint64_t)userdata->packet.duration;

    return r;
}

static int plugin_run_opus(plugin_userdata* userdata, const tag_handler* thandler, const packet_receiver* receiver) {
    int r;
    uint8_t bos = 0;
    uint8_t channel_mapping = 0;
    uint8_t channels = 0;
    unsigned int duration = 0;

    if( (r = getpacket(userdata)) != 0) return r;

    if(userdata->ogg.bos != 0) {
        bos = 1;
    }

    while(bos) {
        if(memcmp(&userdata->packet.data.x[0],"OpusHead",8) == 0) {
            packet_source_reset(&userdata->me);
            if( (r = membuf_copy(&userdata->me.dsi,&userdata->packet.data)) != 0) return r;

            channels = userdata->me.dsi.x[9];

            userdata->me.name = &plugin_name;
            userdata->me.handle = userdata;
            userdata->me.codec = CODEC_TYPE_OPUS;
            userdata->me.sync_flag = 1;
            userdata->me.sample_rate = 48000;
            userdata->me.padding = unpack_u16le(&userdata->me.dsi.x[10]);

            channel_mapping = userdata->me.dsi.x[18];

            switch(channel_mapping) {
                case 0: {
                    switch(channels) {
                        case 1: userdata->me.channel_layout = LAYOUT_MONO; break;
                        case 2: userdata->me.channel_layout = LAYOUT_STEREO; break;
                        default: {
                            LOG1("invalid channel count %u for mapping family 0", channels);
                            return -1;
                        }
                    }
                    break;
                }
                case 1: {
                    switch(channels) {
                        case 1: userdata->me.channel_layout = LAYOUT_MONO; break;
                        case 2: userdata->me.channel_layout = LAYOUT_STEREO; break;
                        case 3: userdata->me.channel_layout = LAYOUT_3_0; break;
                        case 4: userdata->me.channel_layout = LAYOUT_QUAD; break;
                        case 5: userdata->me.channel_layout = LAYOUT_5_0; break;
                        case 6: userdata->me.channel_layout = LAYOUT_5_1; break;
                        case 7: userdata->me.channel_layout = LAYOUT_6_1; break;
                        case 8: userdata->me.channel_layout = LAYOUT_7_1; break;
                        default: {
                            LOG1("invalid channel count %u for mapping family 1", channels);
                            return -1;
                        }
                    }
                    break;
                }
                default: {
                    LOG1("unhandled channel mapping %u", channel_mapping);
                    return -1;
                }
            }

            userdata->packet.sample_rate = userdata->me.sample_rate;
            userdata->packet.sync = 1;
            userdata->packet.pts = 0;
            userdata->packet.pts -= userdata->me.padding;
        } else if(memcmp(&userdata->packet.data.x[0],"OpusTags",8) == 0) {
            if( (r = handle_opus_comment_block(userdata)) != 0) return r;
        } else {
            /* frame header, move on to decoding */
            bos = 0;
            if( (r = receiver->open(receiver->handle, &userdata->me)) != 0) return r;
            if( taglist_len(&userdata->tags) > 0) {
                if( (r = thandler->cb(thandler->userdata,&userdata->tags)) != 0) return r;
            }
            break;
        }

        r = getpacket(userdata);
        if(r != 0) return r;
    }

    duration = opus_get_duration(userdata->packet.data.x, userdata->packet.data.len);
    if(duration == 0 || duration > 5760) {
        LOG1("invalid packet duration: %u", duration);
        return -1;
    }
    userdata->packet.duration = duration;

    if(userdata->ogg.eos != 0) {
        if(userdata->granulepos != ~0ULL) {
            if( (uint64_t)userdata->packet.duration + userdata->granuleoffset + userdata->packet.pts > userdata->granulepos) {
                userdata->packet.duration = userdata->granulepos - (userdata->packet.pts + userdata->granuleoffset);
            }
        }
    }

    r = receiver->submit_packet(receiver->handle, &userdata->packet);
    userdata->packet.pts += (uint64_t) userdata->packet.duration;

    return r;
}


static int plugin_run_unknown(plugin_userdata* userdata, const tag_handler* thandler, const packet_receiver* receiver) {
    int r;
    uint8_t serial = 0; /* flag to track if our serialno value has something */

    /* packet info */
    const uint8_t* packet = NULL;
    size_t packetlen = 0;
    uint64_t granulepos = 0;
    uint8_t cont = 0;

    /* we need to probe for the codec type */
    size_t pages = 0;

    while(pages++ < MAX_PAGES) {
        if( (r = loadpage(userdata)) != 0) return r;

        if(serial == 0) {
            if(userdata->ogg.bos == 0) continue;
            if(userdata->ogg.eos != 0) continue;
            serial = 1;
            userdata->serialno = userdata->ogg.serialno;
        }

        /* we use get_packet instead of iter_packet since we just want to look,
         * the actual plugin_run_(codec) will use iter packet to consume */
        packet = miniogg_get_packet(&userdata->ogg, 0, &packetlen, &granulepos, &cont);
        if(memcmp(packet,"OpusHead",8) == 0) {
            userdata->oggtype = OGG_TYPE_OPUS;
            return plugin_run_opus(userdata,thandler,receiver);
        } else if(memcmp(packet,"\x7F""FLAC""\x01""\x00",7) == 0) {
            userdata->oggtype = OGG_TYPE_FLAC;
            return plugin_run_flac(userdata,thandler,receiver);
        }
        /* no matches, look for a new serialnumber */
        serial = 0;
    }

    /* if we've hit this we've run out of probe pages */
    return -1;
}

static int plugin_run(void* ud, const tag_handler* thandler, const packet_receiver* receiver) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    int (*r)(plugin_userdata*,const tag_handler*,const packet_receiver*) = NULL;

    switch(userdata->oggtype) {
        case OGG_TYPE_UNKNOWN: r = plugin_run_unknown; break;
        case OGG_TYPE_FLAC: r = plugin_run_flac; break;
        case OGG_TYPE_OPUS: r = plugin_run_opus; break;
    }
    return r(userdata,thandler,receiver);
}

const demuxer_plugin demuxer_plugin_ogg = {
    plugin_name,
    plugin_size,
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_run,
};

#define BASE64_DECODE_IMPLEMENTATION
#include "base64decode.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define MINIOGG_IMPLEMENTATION
#include "miniogg.h"
#pragma GCC diagnostic pop
