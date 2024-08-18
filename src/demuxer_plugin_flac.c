#include "demuxer_plugin_flac.h"
#include "unpack_u32be.h"
#include "unpack_u16be.h"
#include "unpack_u32le.h"
#include "base64decode.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#define LOG_PREFIX "[demuxer:auto]"
#include "logger.h"

static STRBUF_CONST(plugin_name, "flac");

/*
 * notes: the first 16 + 4 + 4 + 4 + 3 + 1
 * are semi-fixed - the channel assignment can
 * change between frames, and the block size will
 * be different on the last frame,
 * but the sync code, blocking strategy, sample rate,
 * sample size, and reserved bit should always be
 * the same */

#define HEADER_MASK 0xFFFF0F0F

struct plugin_userdata {
    input* input;
    membuf buffer;
    taglist tags;
    strbuf scratch;
    packet packet;
    uint32_t header_fixed;
    uint8_t empty_tags;
    uint8_t ignore_tags;
    size_t packetno;
    packet_source me;
};

typedef struct plugin_userdata plugin_userdata;

static int plugin_init(void) {
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static size_t plugin_size(void) {
    return sizeof(plugin_userdata);
}

static int plugin_create(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    membuf_init(&userdata->buffer);
    taglist_init(&userdata->tags);
    strbuf_init(&userdata->scratch);
    packet_init(&userdata->packet);

    userdata->header_fixed = 0;
    userdata->empty_tags = 0;
    userdata->ignore_tags = 0;
    userdata->packetno = 0;

    userdata->input = NULL;

    userdata->me = packet_source_zero;

    return 0;
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    membuf_free(&userdata->buffer);
    taglist_free(&userdata->tags);
    strbuf_free(&userdata->scratch);
    packet_free(&userdata->packet);
    packet_source_free(&userdata->me);
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
        log_error("unknown value for key %.*s: %.*s",
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
        log_error("unknown value for key %.*s: %.*s",
          (int)key->len,(char *)key->x,(int)value->len,(char *)value->x);
        return -1;
    }

    log_error("unknown key %.*s",
      (int)key->len,(char *)key->x);
    return -1;
}

static size_t buffer_read(plugin_userdata* userdata, size_t len) {
    size_t r;

    if(membuf_readyplus(&userdata->buffer,len) != 0) return 0;
    r = input_read(userdata->input,&userdata->buffer.x[userdata->buffer.len],len);
    userdata->buffer.len += r;
    return r;
}

static int handle_picture_block(plugin_userdata* userdata, uint32_t len) {
    strbuf key = STRBUF_ZERO;
    strbuf val = STRBUF_ZERO;

    if(len <= 4) return 0;

    key.x = (uint8_t*)"metadata_block_picture";
    key.len = strlen("metadata_block_picture");

    val.x = &userdata->buffer.x[4];
    val.len = len - 4;

    return taglist_add(&userdata->tags,&key,&val);
}

static int handle_comment_block(plugin_userdata* userdata, uint32_t len) {
    int r;
    size_t i = 4;
    size_t c = 0;
    uint32_t comments = 0;

    strbuf key = STRBUF_ZERO;
    strbuf val = STRBUF_ZERO;
    strbuf eq  = STRBUF_ZERO;

    comments = unpack_u32le(&userdata->buffer.x[i]);
    if(comments == 0) return 0;
    i += 4 + comments;
    if(i >= len) return 0;

    comments = unpack_u32le(&userdata->buffer.x[i]);
    i += 4;

    for(c=0;c<comments;c++) {
        if(i == len) break;

        key.len = unpack_u32le(&userdata->buffer.x[i]);
        i += 4;
        if(i > len) break;

        key.x = &userdata->buffer.x[i];
        i += key.len;
        if(i > len) break;

        if(strbuf_chrbuf(&eq,&key,'=') != 0) continue;
        val.x = &eq.x[1];
        val.len = eq.len - 1;
        key.len -= val.len + 1;

        if(key.len == 0) continue;
        if(val.len == 0 && !userdata->empty_tags) continue;
        strbuf_lower(&key);

        log_debug("comment: %.*s=%.*s",
          (int)key.len,(const char *)key.x,
          (int)val.len,(const char *)val.x);

        if(strbuf_equals_cstr(&key,"metadata_block_picture")) {
            /* base64-decode the picture block */
            if( (r = membuf_ready(&userdata->scratch,val.len) != 0)) {
                logs_fatal("failed to allocate image buffer");
                return r;
            }
            userdata->scratch.len = val.len;
            if( (r = base64decode(val.x,val.len,userdata->scratch.x,&userdata->scratch.len)) != 0) {
                log_error("base64 decode failed: %d",r);
                return r;
            }
            val.x = userdata->scratch.x;
            val.len = userdata->scratch.len;
        } else if(strbuf_equals_cstr(&key,"waveformatextensible_channel_mask")) {
            userdata->me.channel_layout = strbuf_strtoull(&val, 16);
            log_debug("setting channel mask to 0x%" PRIx64, userdata->me.channel_layout);
            continue;
        }

        if(!userdata->ignore_tags) {
            if( (r = taglist_add(&userdata->tags,&key,&val)) != 0) return r;
        }
    }

    return 0;
}

static int plugin_open(void* ud, input* in) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    userdata->input = in;

    if( buffer_read(userdata, 4) != 4) {
        return -1;
    }

    if(userdata->buffer.x[0] != 'f' ||
       userdata->buffer.x[1] != 'L' ||
       userdata->buffer.x[2] != 'a' ||
       userdata->buffer.x[3] != 'C') {
        logs_error("missing fLaC stream marker");
        return -1;
    }

    membuf_trim(&userdata->buffer,4);
    return 0;
}


static int plugin_run(void* ud, const tag_handler* thandler, const packet_receiver* receiver) {
    int r;
    uint32_t type;
    uint32_t len;
    uint32_t rem;
    uint16_t min_block_size;
    uint16_t max_block_size;
    uint8_t channels;
    uint32_t t;
    size_t i;
    size_t j;
    size_t got;
    uint8_t have_data = 1;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(userdata->header_fixed == 0) {
        do {
            if( buffer_read(userdata, 4) != 4) {
                return -1;
            }
            len = unpack_u32be(&userdata->buffer.x[0]);

            type = (len >> 24) & 0xFF;
            if(type == 0xFF) {
                userdata->header_fixed = len & HEADER_MASK;
                break;
            }

            type &= 0x7F;
            if(type == 0x7F) {
                log_error("invalid block header type 0x%02x",type);
                return -1;
            }

            len  &= 0x00FFFFFF;
            rem = len;
            while(rem) {
                got = buffer_read(userdata, rem);
                if(!got) break;
                rem -= got;
            }
            if(rem != 0) {
                logs_error("error filling buffer");
                return -1;
            }

            switch(type) {
                case 0: {
                    if( (r = membuf_append(&userdata->me.dsi,&userdata->buffer.x[4], len)) != 0) return r;
                    break;
                }
                case 4: {
                    if( (r = handle_comment_block(userdata, 4+len)) != 0) return r;
                    break;
                }
                case 6: {
                    if( (r = handle_picture_block(userdata, 4+len)) != 0) return r;
                    break;
                }
                default: break;
            }

            membuf_trim(&userdata->buffer,4 + len);
        } while(userdata->header_fixed == 0);

        if(userdata->me.dsi.len == 0) {
            logs_error("didn't get STREAMINFO block");
            return -1;
        }

        min_block_size = unpack_u16be(&userdata->me.dsi.x[0]);
        max_block_size = unpack_u16be(&userdata->me.dsi.x[2]);
        channels       = ((userdata->me.dsi.x[12] >> 1) & 0x07) + 1;

        if(min_block_size == max_block_size) userdata->me.frame_len = min_block_size;

        userdata->me.name = &plugin_name;
        userdata->me.handle = userdata;
        userdata->me.codec = CODEC_TYPE_FLAC;
        userdata->me.sample_rate = unpack_u32be(&userdata->me.dsi.x[10]) >> 12;
        userdata->me.sync_flag = 1;
        if(userdata->me.channel_layout == 0) {
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
        }

        userdata->packet.sample_rate = userdata->me.sample_rate;
        userdata->packet.sync = 1;
        userdata->packet.pts = 0;

        if( (r = receiver->open(receiver->handle, &userdata->me)) != 0) return r;

        if(taglist_len(&userdata->tags) > 0) {
            if( (r = thandler->cb(thandler->userdata, &userdata->tags)) != 0) return r;
        }
    }

    if(userdata->buffer.len == 0 &&
       buffer_read(userdata,1<<17) == 0) return 1;

    t = unpack_u32be(&userdata->buffer.x[0])  & HEADER_MASK;
    if(t != userdata->header_fixed) {
        logs_error("had some kind of sync issue");
        return -1;
    }

    i = 6; /* minimum heade size is 6 bytes */
    while(have_data) {
        while(i < userdata->buffer.len) {
            t = unpack_u32be(&userdata->buffer.x[i]) & HEADER_MASK;
            if(t == userdata->header_fixed) {
                goto done;
            }
            i++;
        }
        have_data = buffer_read(userdata,1<<17) != 0;
    }
    done:

    /* if we hit EOF we just assume this is the last frame */

    userdata->packet.data.len = 0;
    if( (r = membuf_append(&userdata->packet.data, &userdata->buffer.x[0], i)) != 0) return r;

    userdata->packet.duration = (userdata->buffer.x[2] >> 4) & 0x0F;
    switch(userdata->packet.duration) {
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
            j = 4;
            if( (userdata->buffer.x[4] & 0x80) == 0x00) {
                j += 1;
            } else if( (userdata->buffer.x[4] & 0xE0) == 0xC0) {
                j += 2;
            } else if( (userdata->buffer.x[4] & 0xF0) == 0xE0) {
                j += 3;
            } else if( (userdata->buffer.x[4] & 0xF8) == 0xF0) {
                j += 4;
            } else if( (userdata->buffer.x[4] & 0xFC) == 0xF8) {
                j += 5;
            } else if( (userdata->buffer.x[4] & 0xFE) == 0xFC) {
                j += 6;
            } else if( (userdata->buffer.x[4] & 0xFF) == 0xFE) {
                j += 7;
            }
            userdata->packet.duration = userdata->buffer.x[j] + 1;
            break;
        }
        case 7: {
            j = 4;
            if( (userdata->buffer.x[4] & 0x80) == 0x00) {
                j += 1;
            } else if( (userdata->buffer.x[4] & 0xE0) == 0xC0) {
                j += 2;
            } else if( (userdata->buffer.x[4] & 0xF0) == 0xE0) {
                j += 3;
            } else if( (userdata->buffer.x[4] & 0xF8) == 0xF0) {
                j += 4;
            } else if( (userdata->buffer.x[4] & 0xFC) == 0xF8) {
                j += 5;
            } else if( (userdata->buffer.x[4] & 0xFE) == 0xFC) {
                j += 6;
            } else if( (userdata->buffer.x[4] & 0xFF) == 0xFE) {
                j += 7;
            }
            userdata->packet.duration = unpack_u16be(&userdata->buffer.x[j]) + 1;
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
    membuf_trim(&userdata->buffer,i);

    return r;
}


const demuxer_plugin demuxer_plugin_flac = {
    &plugin_name,
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
