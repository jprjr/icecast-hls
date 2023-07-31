#include "muxer_plugin_ogg_flac.h"
#include "muxer_caps.h"

#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <inttypes.h>

#include "pack_u32le.h"
#include "pack_u32be.h"

#define LOG_PREFIX "[muxer:ogg:flac]"
#include "logger.h"

#define MINIOGG_API static
#include "miniogg.h"

#include "base64encode.h"

#define LOGS(s, a) log_error(s, (int)(a).len, (const char *)(a).x )

#define LOGERRNO(s) log_error(s": %s", strerror(errno))

#define TRYNULL(exp, act) if( (exp) == NULL) { act; r=-1; goto cleanup; }
#define TRY(exp, act) if(!(exp)) { act; r=-1; goto cleanup ; }
#define TRY0(exp, act) if( (r = (exp)) != 0 ) { act; goto cleanup; }
#define TRYS(exp) TRY0(exp, logs_error("out of memory"); abort())

static STRBUF_CONST(plugin_name,"ogg:flac");
static STRBUF_CONST(mime_ogg,"application/ogg");
static STRBUF_CONST(ext_ogg,".ogg");

static const uint8_t oggflac_header[13] = {
    0x7F,
    'F', 'L', 'A', 'C',
    0x01, 0x00,
    0x00, 0x01, /* we'll always have a VORBIS_COMMENT packet */
    'f','L','a','C'
};



struct ogg_flac_plugin {
    unsigned int padding;
    uint64_t samples_per_segment;

    strbuf head;
    strbuf tags;
    uint32_t tagpos;
    uint32_t tagtotal;
    strbuf segment;
    miniogg ogg;
    uint64_t pts;
    uint64_t granulepos;
    uint64_t samples;
    uint64_t channel_layout;
    uint8_t flag;
    strbuf scratch;

    uint8_t chaining;
};

typedef struct ogg_flac_plugin ogg_flac_plugin;

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

static int stream_send(ogg_flac_plugin* stream, const segment_receiver* dest) {
    segment s;
    int r = -1;

    s.type    = SEGMENT_TYPE_MEDIA;
    s.data    = stream->segment.x;
    s.len     = stream->segment.len;
    s.samples = stream->samples;
    s.pts     = stream->pts;

    TRY0(dest->submit_segment(dest->handle,&s),logs_error("error submitting segment"));

    stream->pts += stream->samples;
    stream->samples = 0;
    stream->segment.len = 0;

    r = 0;
    cleanup:
    return r;
}

static int stream_end(ogg_flac_plugin* stream) {
    int r = -1;

    miniogg_eos(&stream->ogg);
    TRYS(membuf_append(&stream->segment,stream->ogg.header,stream->ogg.header_len));
    TRYS(membuf_append(&stream->segment,stream->ogg.body,stream->ogg.body_len));

    r = 0;
    cleanup:
    return r;
}

static int stream_buffer(ogg_flac_plugin* stream) {
    int r = -1;

    miniogg_finish_page(&stream->ogg);
    TRYS(membuf_append(&stream->segment,stream->ogg.header,stream->ogg.header_len));
    TRYS(membuf_append(&stream->segment,stream->ogg.body,stream->ogg.body_len));

    r = 0;
    cleanup:
    return r;
}

static int stream_add_strbuf(ogg_flac_plugin* stream, const strbuf* data,uint64_t granulepos) {
    int r = -1;
    size_t pos; size_t used; size_t len;

    len = data->len;
    used = 0;
    pos = 0;

    while(miniogg_add_packet(&stream->ogg,&data->x[pos],len,granulepos,&used)) {
        TRYS(stream_buffer(stream));

        pos += used;
        len -= used;
    }

    r = 0;
    cleanup:
    return r;
}

static size_t plugin_size(void) {
    return sizeof(ogg_flac_plugin);
}

static int plugin_create(void* ud) {
    ogg_flac_plugin *userdata = (ogg_flac_plugin*)ud;

    userdata->chaining = 1;

    userdata->samples_per_segment = 0;

    strbuf_init(&userdata->head);
    strbuf_init(&userdata->tags);
    strbuf_init(&userdata->segment);
    strbuf_init(&userdata->scratch);

    userdata->tagpos = 0;
    userdata->tagtotal = 0;
    userdata->granulepos = 0;
    userdata->samples = 0;
    userdata->pts = 0;
    userdata->flag  = 0;

    miniogg_init(&userdata->ogg,(uint32_t)rand());

    return 0;
}

static int plugin_reset(void* ud) {
    ogg_flac_plugin* userdata = (ogg_flac_plugin*)ud;

    strbuf_init(&userdata->head);
    strbuf_init(&userdata->tags);
    strbuf_init(&userdata->segment);
    strbuf_init(&userdata->scratch);

    userdata->tagpos = 0;
    userdata->tagtotal = 0;
    userdata->granulepos = 0;
    userdata->samples = 0;
    userdata->pts = 0;
    userdata->flag  = 0;

    miniogg_init(&userdata->ogg,userdata->ogg.serialno + 1);
    return 0;
}

static void plugin_close(void* ud) {
    ogg_flac_plugin* userdata = (ogg_flac_plugin*)ud;

    strbuf_free(&userdata->head);
    strbuf_free(&userdata->tags);
    strbuf_free(&userdata->segment);
    strbuf_free(&userdata->scratch);
}

static int plugin_open(void* ud, const packet_source* source, const segment_receiver* dest) {
    int r = -1;
    ogg_flac_plugin* userdata = (ogg_flac_plugin*)ud;

    segment_source me = SEGMENT_SOURCE_ZERO;
    segment_source_info info = SEGMENT_SOURCE_INFO_ZERO;
    segment_params s_params = SEGMENT_PARAMS_ZERO;

    userdata->pts = 0 - userdata->padding;
    userdata->flag = 0;

    info.time_base = source->sample_rate;
    info.frame_len = source->frame_len;
    dest->get_segment_info(dest->handle,&info,&s_params);

    userdata->samples_per_segment = s_params.segment_length * source->sample_rate / 1000;
    userdata->channel_layout = source->channel_layout;

    /* setup and buffer the FLAC header packet */
    userdata->head.len = 0;
    TRYS(membuf_append(&userdata->head,oggflac_header,13));
    TRYS(membuf_readyplus(&userdata->head,4));
    pack_u32be(&userdata->head.x[13],34);
    userdata->head.len += 4;
    TRYS(strbuf_cat(&userdata->head,&source->dsi));
    userdata->head.x[13] &= 0x7F; /* clear the last-metadata-block flag */

    TRYS(stream_add_strbuf(userdata,&userdata->head,0));
    TRYS(stream_buffer(userdata));

    /* prep the tag buffer,
     * hold off on buffering until we get tags or a packet */
    TRYS(strbuf_ready(&userdata->tags,4));
    userdata->tags.len = 4;

    if(source->name == NULL) {
        TRYS(ogg_pack_cstr(&userdata->tags,"icecast-hls"));
    } else {
        TRYS(ogg_pack_str(&userdata->tags,(const char *)source->name->x,source->name->len));
    }

    /* reserve 4 bytes for writing the total number of tags */
    TRYS(strbuf_readyplus(&userdata->tags,4));
    userdata->tagpos = userdata->tags.len;
    userdata->tagtotal = 0;
    userdata->tags.len += 4;

    switch(userdata->channel_layout) {
        case LAYOUT_MONO: /* fall-through */
        case LAYOUT_STEREO: /* fall-through */
        case LAYOUT_3_0: /* fall-through */
        case LAYOUT_QUAD: /* fall-through */
        case LAYOUT_5_0: /* fall-through */
        case LAYOUT_5_1: /* fall-through */
        case LAYOUT_6_1: /* fall-through */
        case LAYOUT_7_1: break;
        default: {
            userdata->scratch.len = 0;
            TRYS(strbuf_sprintf(&userdata->scratch,"WAVEFORMATEXTENSIBLE_CHANNEL_MASK=0x%llx",
              userdata->channel_layout));
            TRYS(ogg_pack_str(&userdata->tags,(const char *)userdata->scratch.x,userdata->scratch.len));
            userdata->tagtotal++;
            break;
        }
    }

    pack_u32be(&userdata->tags.x[0],(0x84 << 24) | (userdata->tags.len - 4));
    pack_u32le(&userdata->tags.x[userdata->tagpos],userdata->tagtotal);
    userdata->padding = source->padding;
    userdata->pts = 0 - source->padding;

    me.media_ext = &ext_ogg;
    me.media_mimetype = &mime_ogg;
    me.time_base = source->sample_rate;
    me.frame_len = source->frame_len;
    me.handle = userdata;

    TRY0(dest->open(dest->handle,&me),logs_error("error opening destination"));

    r = 0;
    cleanup:
    return r;
}

static int plugin_submit_tags(void* ud, const taglist* tags, const segment_receiver* dest) {
    int r = -1;
    ogg_flac_plugin* userdata = (ogg_flac_plugin*)ud;
    strbuf* tagdata;

    const tag* t;
    size_t i = 0;
    size_t m = 0;
    size_t len = 0;

    tagdata = &userdata->tags;
    tagdata->len = userdata->tagpos + 4; /* we're going to rewrite all tags */
    userdata->tagtotal = 0;

    switch(userdata->channel_layout) {
        case LAYOUT_MONO: /* fall-through */
        case LAYOUT_STEREO: /* fall-through */
        case LAYOUT_3_0: /* fall-through */
        case LAYOUT_QUAD: /* fall-through */
        case LAYOUT_5_0: /* fall-through */
        case LAYOUT_5_1: /* fall-through */
        case LAYOUT_6_1: /* fall-through */
        case LAYOUT_7_1: break;
        default: {
            if(!userdata->chaining) {
                logs_error("ogg is set to non-chaining mode but audio channel layout requires chaining");
                return -1;
            }
            userdata->scratch.len = 0;
            TRYS(strbuf_sprintf(&userdata->scratch,"WAVEFORMATEXTENSIBLE_CHANNEL_MASK=0x%" PRIx64,
              userdata->channel_layout));
            TRYS(ogg_pack_str(tagdata,(const char *)userdata->scratch.x,userdata->scratch.len));
            userdata->tagtotal++;
            break;
        }
    }

    if(!userdata->chaining) {
        return dest->submit_tags(dest->handle,tags);
    }


    if(tags != NULL) m = taglist_len(tags);
    for(i=0;i<m;i++) {
        t = taglist_get_tag(tags,i);

        userdata->scratch.len = 0;
        TRYS(strbuf_copy(&userdata->scratch,&t->key));
        TRYS(strbuf_append_cstr(&userdata->scratch,"="));
        if(strbuf_caseequals_cstr(&t->key,"metadata_block_picture")) {
            len = t->value.len * 4 / 3 + 4;
            TRYS(strbuf_readyplus(&userdata->scratch,len));
            base64encode(t->value.x,t->value.len,&userdata->scratch.x[userdata->scratch.len],&len);
            userdata->scratch.len += len;
        } else {
            TRYS(strbuf_cat(&userdata->scratch,&t->value));
        }
        TRYS(ogg_pack_str(tagdata,(const char *)userdata->scratch.x,userdata->scratch.len));

        userdata->tagtotal++;
    }
    pack_u32be(&tagdata->x[0],(0x84 << 24) | (tagdata->len - 4));
    pack_u32le(&tagdata->x[userdata->tagpos],userdata->tagtotal);

    TRYS(stream_add_strbuf(userdata,&userdata->tags,0));
    TRYS(stream_buffer(userdata));

    userdata->flag = 1;

    r = 0;
    cleanup:
    return r;
}

static int plugin_submit_packet(void* ud, const packet* p, const segment_receiver* dest) {
    int r = -1;

    ogg_flac_plugin* userdata = (ogg_flac_plugin*)ud;

    if(userdata->flag == 0 && userdata->chaining) {
        /* getting a packet before tags, add our blank flactags block */
        if( (r = plugin_submit_tags(userdata, NULL, dest)) != 0) return r;
    }

    userdata->granulepos += p->duration;
    TRYS(stream_add_strbuf(userdata,&p->data,userdata->granulepos));
    userdata->samples += p->duration;

    if(userdata->samples >= userdata->samples_per_segment) {
        TRYS(stream_buffer(userdata));
        TRYS(stream_send(userdata,dest));
    }


    r = 0;
    cleanup:
    return r;
}

static int plugin_flush(void* ud, const segment_receiver* dest) {
    int r = -1;

    ogg_flac_plugin* userdata = (ogg_flac_plugin*)ud;

    if(userdata->flag == 0) {
        /* getting a flush before we ever got tags / sent anything, just close */
        return 0;
    }

    TRYS(stream_end(userdata));
    TRYS(stream_send(userdata,dest));

    r = 0;
    cleanup:
    return r;
}


static int plugin_config(void* ud, const strbuf* key, const strbuf* value) {
    ogg_flac_plugin* userdata = (ogg_flac_plugin*)ud;

    if(strbuf_equals_cstr(key,"chaining")) {
        if(strbuf_truthy(value)) {
            userdata->chaining = 1;
            return 0;
        }
        if(strbuf_falsey(value)) {
            userdata->chaining = 0;
            return 0;
        }
        LOGS("unsupported value for chaining: %.*s",(*value));
        return -1;
    }

    LOGS("unknown key %.*s",(*key));
    return -1;
}

static int plugin_init(void) {
    srand(time(NULL));
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static uint32_t plugin_get_caps(void* ud) {
    ogg_flac_plugin* userdata = (ogg_flac_plugin*)ud;
    uint32_t caps = MUXER_CAP_GLOBAL_HEADERS;
    if(userdata->samples > 0) caps |= MUXER_CAP_TAGS_RESET;
    return caps;
}

static int plugin_get_segment_info(const void* ud, const packet_source_info* s, const segment_receiver* dest, packet_source_params* i) {
    (void)ud;
    (void)s;
    (void)dest;
    (void)i;
    return 0;
}

const muxer_plugin muxer_plugin_ogg_flac = {
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define MINIOGG_IMPLEMENTATION
#include "miniogg.h"
#pragma GCC diagnostic pop

#define BASE64_ENCODE_IMPLEMENTATION
#include "base64encode.h"
