#include "muxer_plugin_flac.h"
#include "muxer_caps.h"

#include <string.h>
#include <errno.h>

#define LOG_PREFIX "[muxer:flac]"
#include "logger.h"

#define LOGERRNO(s) log_error(s": %s", strerror(errno))

#include "pack_u32le.h"
#include "pack_u32be.h"

#include "unpack_u32le.h"

static STRBUF_CONST(plugin_name,"flac");
static STRBUF_CONST(mime_flac,"audio/flac");
static STRBUF_CONST(ext_flac,".flac");

struct plugin_userdata {
    membuf buffer;
    uint64_t samples;
    uint64_t samples_per_segment;
};

typedef struct plugin_userdata plugin_userdata;

static int ogg_prep_str(strbuf* dest, size_t len) {
    int r;
    if( (r = strbuf_readyplus(dest,4+len)) != 0) return r;
    pack_u32le(&dest->x[dest->len],len);
    dest->len += 4;
    return 0;
}

static int ogg_pack_str(strbuf* dest, const char* str, size_t len) {
    int r;
    if( (r = ogg_prep_str(dest,len)) != 0) return r;
    return strbuf_append(dest,str,len);
}

static int ogg_pack_cstr(strbuf* dest, const char* str) {
    return ogg_pack_str(dest,str,strlen(str));
}

static size_t plugin_size(void) {
    return sizeof(plugin_userdata);
}

static int plugin_create(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    membuf_init(&userdata->buffer);
    userdata->samples = 0;
    userdata->samples_per_segment = 0;
    return 0;
}

static void plugin_close(void *ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    membuf_free(&userdata->buffer);
    return;
}

static int plugin_open(void *ud, const packet_source* source, const segment_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    segment_source me = SEGMENT_SOURCE_ZERO;
    segment_source_info info = SEGMENT_SOURCE_INFO_ZERO;
    segment_params s_params = SEGMENT_PARAMS_ZERO;
    int r = 0;
    uint32_t u;

    switch(source->codec) {
        case CODEC_TYPE_FLAC: break;
        default: {
            log_error("unsupported codec %s",codec_name(source->codec));
            return -1;
        }
    }

    info.time_base = source->sample_rate;
    info.frame_len = source->frame_len;
    dest->get_segment_info(dest->handle, &info, &s_params);
    userdata->samples_per_segment = s_params.segment_length * source->sample_rate / 1000;

    me.handle = userdata;
    me.media_ext = &ext_flac;
    me.media_mimetype = &mime_flac;
    me.time_base = source->sample_rate;
    me.frame_len = source->frame_len;

    /* fLaC stream marker */
    if( (r = membuf_append(&userdata->buffer,"fLaC",4)) != 0) return r;

    /* STREAMINFO block */
    u = (uint32_t) source->dsi.len;

    if( (r = membuf_readyplus(&userdata->buffer,4)) != 0) return r;
    pack_u32be(&userdata->buffer.x[userdata->buffer.len], u);
    userdata->buffer.len += 4;

    if( (r = membuf_cat(&userdata->buffer,&source->dsi)) != 0) return r;

    /* VORBIS_COMMENT block */
    /* make room for header */
    if( (r = membuf_readyplus(&userdata->buffer,4)) != 0) return r;
    pack_u32be(&userdata->buffer.x[userdata->buffer.len],0);
    userdata->buffer.len += 4;

    /* write out the vendor */
    if( source->name  == NULL) {
        if( (r = ogg_pack_cstr(&userdata->buffer,"icecast-hls")) != 0) return r;
    } else {
        if( (r = ogg_pack_str(&userdata->buffer,(const char *)source->name->x, source->name->len)) != 0) return r;
    }

    /* make room for number of tags */
    if( (r = membuf_readyplus(&userdata->buffer, 4)) != 0) return r;
    pack_u32be(&userdata->buffer.x[userdata->buffer.len],0);
    userdata->buffer.len += 4;

    /* write size of block to the header */
    u = userdata->buffer.len - 4 - 4 - 34 - 4;
    u |= 0x84000000;
    pack_u32be(&userdata->buffer.x[4 + 4 + 34],u);

    if( (r = dest->open(dest->handle, &me)) != 0) {
        return r;
    }

    return 0;
}

static int plugin_submit_tags(void* ud, const taglist* tags, const segment_receiver* dest) {
    (void)tags;
    (void)dest;
    plugin_userdata* userdata = (plugin_userdata*)ud;
    const tag* t = NULL;
    const tag* picture = NULL;
    size_t i = 0;
    size_t m = 0;
    size_t tot = 0;
    size_t tagpos = 0;
    uint32_t len = 0;
    int r;

    if(tags != NULL) m = taglist_len(tags);

    /* get the total number of tags */
    tagpos = 4 + 4 + 34 + 4; /* location of vendor length value */
    len = unpack_u32le(&userdata->buffer.x[tagpos]);
    tagpos += 4 + (size_t)len;
    tot = unpack_u32le(&userdata->buffer.x[tagpos]);

    for(i=0;i<m;i++) {
        t = taglist_get_tag(tags,i);
        if(strbuf_caseequals_cstr(&t->key,"metadata_block_picture")) {
            picture = t;
            continue;
        }

        if( (r = membuf_readyplus(&userdata->buffer,4)) != 0) return r;
        len = t->key.len + t->value.len + 1;
        pack_u32le(&userdata->buffer.x[userdata->buffer.len], len);
        userdata->buffer.len += 4;
        if( ( r = membuf_cat(&userdata->buffer, &t->key)) != 0) return r;
        if( ( r = membuf_append(&userdata->buffer, "=", 1)) != 0) return r;
        if( ( r = membuf_cat(&userdata->buffer, &t->value)) != 0) return r;
        tot++;
    }

    /* update the total number of tags */
    pack_u32le(&userdata->buffer.x[tagpos],((uint32_t)tot));

    /* update the block header */
    len = userdata->buffer.len - 4 - 4 - 34 - 4;
    len |= 0x04000000;
    if(picture == NULL) {
        len |= 0x84000000;
    }
    pack_u32be(&userdata->buffer.x[4 + 4 + 34],len);

    if(picture != NULL) {
        if( (r = membuf_readyplus(&userdata->buffer,4)) != 0) return r;
        len = t->value.len;
        len |= 0x86000000;
        pack_u32be(&userdata->buffer.x[userdata->buffer.len], len);
        userdata->buffer.len += 4;
        if( (r = membuf_cat(&userdata->buffer, &t->value)) != 0) return r;
    }

    return 0;
}

static int plugin_submit_packet(void* ud, const packet* packet, const segment_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    segment s = SEGMENT_ZERO;
    int r = 0;

    if( (r = membuf_cat(&userdata->buffer,&packet->data)) != 0) {
        LOGERRNO("error appending to memory buffer");
        return r;
    }
    userdata->samples += packet->duration;

    if(userdata->samples >= userdata->samples_per_segment) {
        s.type = SEGMENT_TYPE_MEDIA;
        s.data = userdata->buffer.x;
        s.len = userdata->buffer.len;
        s.samples = packet->duration;
        s.pts = packet->pts;

        if( (r = dest->submit_segment(dest->handle, &s)) != 0) {
            return r;
        }

        membuf_reset(&userdata->buffer);
        userdata->samples = 0;
    }

    return 0;
}

static int plugin_reset(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    membuf_reset(&userdata->buffer);
    userdata->samples = 0;
    userdata->samples_per_segment = 0;
    return 0;
}

static int plugin_flush(void *ud, const segment_receiver* dest) {
    (void)ud;
    (void)dest;
    return 0;
}

static int plugin_init(void) { return 0; }
static void plugin_deinit(void) { return; }
static int plugin_config(void* ud, const strbuf* key, const strbuf* val) {
    (void)ud;
    (void)key;
    (void)val;
    return 0;
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

static uint32_t plugin_get_caps(void *ud) {
    (void) ud;
    return MUXER_CAP_GLOBAL_HEADERS;
}

const muxer_plugin muxer_plugin_flac = {
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
