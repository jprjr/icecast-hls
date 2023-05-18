#include "muxer_plugin_ogg_flac.h"

#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#include "pack_u32le.h"
#include "pack_u32be.h"

#define MINIOGG_API static
#include "miniogg.h"

#include "base64encode.h"

#define LOG0(s) fprintf(stderr,"[muxer:ogg:flac] "s"\n")
#define LOG1(s, a) fprintf(stderr,"[muxer:ogg:flac] "s"\n", (a))
#define LOG2(s, a, b) fprintf(stderr,"[muxer:ogg:flac] "s"\n", (a), (b))
#define LOGS(s, a) LOG2(s, (int)(a).len, (const char *)(a).x )

#define LOGERRNO(s) LOG1(s": %s", strerror(errno))

#define TRYNULL(exp, act) if( (exp) == NULL) { act; r=-1; goto cleanup; }
#define TRY(exp, act) if(!(exp)) { act; r=-1; goto cleanup ; }
#define TRY0(exp, act) if( (r = (exp)) != 0 ) { act; goto cleanup; }
#define TRYS(exp) TRY0(exp, LOG0("out of memory"); abort())

static STRBUF_CONST(mime_ogg,"application/ogg");
static STRBUF_CONST(ext_ogg,".ogg");

struct ogg_flac_plugin {
    unsigned int padding;
    unsigned int keyframes;
    packet_source psource;
    uint64_t samples_per_segment;
    segment_source_params segment_params;

    strbuf head;
    strbuf tags;
    uint32_t tagpos;
    strbuf segment;
    miniogg ogg;
    uint64_t pts;
    uint64_t granulepos;
    uint64_t samples;
    uint8_t flag;
    strbuf scratch;
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

    TRY0(dest->submit_segment(dest->handle,&s),LOG0("error submitting segment"));

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

static void* plugin_create(void) {
    ogg_flac_plugin *userdata = (ogg_flac_plugin*)malloc(sizeof(ogg_flac_plugin));
    if(userdata == NULL) return NULL;

    userdata->keyframes = 0;
    userdata->psource = packet_source_zero;
    userdata->segment_params = segment_source_params_zero;
    userdata->samples_per_segment = 0;

    strbuf_init(&userdata->head);
    strbuf_init(&userdata->tags);
    strbuf_init(&userdata->segment);
    strbuf_init(&userdata->scratch);
    userdata->tagpos = 0;
    userdata->granulepos = 0;
    userdata->samples = 0;
    userdata->pts = 0;
    miniogg_init(&userdata->ogg,(uint32_t)rand());
    return userdata;
}

static void plugin_close(void* ud) {
    ogg_flac_plugin* userdata = (ogg_flac_plugin*)ud;

    strbuf_free(&userdata->head);
    strbuf_free(&userdata->tags);
    strbuf_free(&userdata->segment);
    strbuf_free(&userdata->scratch);
    free(userdata);
}

static int receive_params(void* ud, const segment_source_params* params) {
    ogg_flac_plugin* userdata = (ogg_flac_plugin*)ud;
    userdata->segment_params = *params;
    return 0;
}

static int plugin_open(void* ud, const packet_source* source, const segment_receiver* dest) {
    int r = -1;
    uint64_t samples_per_segment = 0;
    ogg_flac_plugin* userdata = (ogg_flac_plugin*)ud;

    /* prep the tag buffer */
    TRYS(strbuf_ready(&userdata->tags,4));
    userdata->tags.len = 4;

    if(source->name == NULL) {
        TRYS(ogg_pack_cstr(&userdata->tags,"icecast-hls"));
    } else {
        TRYS(ogg_pack_str(&userdata->tags,(const char *)source->name->x,source->name->len));
    }
    TRYS(strbuf_readyplus(&userdata->tags,4));
    userdata->tagpos = userdata->tags.len;
    userdata->tags.len += 4;

    pack_u32be(&userdata->tags.x[0],(0x84 << 24) | (userdata->tags.len - 4));
    pack_u32le(&userdata->tags.x[userdata->tagpos],0);
    userdata->padding = source->padding;
    userdata->pts = 0 - source->padding;

    userdata->psource = *source;

    segment_source me = SEGMENT_SOURCE_ZERO;
    packet_source_params params = PACKET_SOURCE_PARAMS_ZERO;

    me.media_ext = &ext_ogg;
    me.media_mimetype = &mime_ogg;
    me.time_base = source->sample_rate;
    me.frame_len = source->frame_len;

    me.handle = userdata;
    me.set_params = receive_params;

    TRY0(dest->open(dest->handle,&me),LOG0("error opening destination"));

    if(userdata->segment_params.segment_length == 0) {
        /* output plugin didn't set a length so we'll set a default */
        userdata->segment_params.segment_length = 1000;
    }

    if(userdata->segment_params.packets_per_segment == 0) {
        samples_per_segment  = (uint64_t) userdata->segment_params.segment_length;
        samples_per_segment *= (uint64_t) source->sample_rate;
        samples_per_segment /= (uint64_t) 1000;
        userdata->samples_per_segment = samples_per_segment;
        userdata->segment_params.packets_per_segment = (size_t)samples_per_segment / (size_t)source->frame_len;
    } else {
        userdata->samples_per_segment = (uint64_t) userdata->segment_params.packets_per_segment;
        userdata->samples_per_segment *= (uint64_t) source->frame_len;
    }

    if(userdata->segment_params.packets_per_segment == 0) {
        userdata->segment_params.packets_per_segment = 1;
    }

    if(userdata->samples_per_segment == 0) {
        userdata->samples_per_segment = userdata->segment_params.packets_per_segment * source->frame_len;
    }

    params.packets_per_segment = userdata->segment_params.packets_per_segment;

    TRY0(source->set_params(source->handle,&params),LOG0("error setting source params"));

    r = 0;
    cleanup:
    return r;
}

static const uint8_t oggflac_header[13] = {
    0x7F,
    'F', 'L', 'A', 'C',
    0x01, 0x00,
    0x00, 0x01, /* we'll always have a VORBIS_COMMENT packet */
    'f','L','a','C'
};

static int plugin_submit_dsi(void* ud, const strbuf* data, const segment_receiver* dest) {
    int r = -1;

    ogg_flac_plugin* userdata = (ogg_flac_plugin*)ud;

    userdata->head.len = 0;
    TRYS(membuf_append(&userdata->head,oggflac_header,13));
    TRYS(strbuf_cat(&userdata->head,data));
    userdata->head.x[13] &= 0x7F; /* clear the last-metadata-block flag */

    TRYS(stream_add_strbuf(userdata,&userdata->head,0));
    TRYS(stream_buffer(userdata));

    r = 0;
    cleanup:
    (void)dest;
    return 0;
}

static int plugin_submit_packet(void* ud, const packet* p, const segment_receiver* dest) {
    int r = -1;

    ogg_flac_plugin* userdata = (ogg_flac_plugin*)ud;

    if(userdata->flag == 0) {
        /* getting a packet before tags, add our blank flactags block */
        TRYS(stream_add_strbuf(userdata,&userdata->tags,0));
        TRYS(stream_buffer(userdata));
        userdata->flag = 1;
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
        return dest->flush(dest->handle);
    }

    TRYS(stream_end(userdata));
    TRYS(stream_send(userdata,dest));

    r = dest->flush(dest->handle);
    cleanup:
    return r;
}

typedef struct reset_wrapper {
    ogg_flac_plugin* userdata;
    const segment_receiver* dest;
} reset_wrapper;

static int reset_cb(void* ud, const packet* p) {
    reset_wrapper* wrap = (reset_wrapper*)ud;
    return plugin_submit_packet(wrap->userdata, p, wrap->dest);
}

static int plugin_submit_tags(void* ud, const taglist* tags, const segment_receiver* dest) {
    int r = -1;
    ogg_flac_plugin* userdata = (ogg_flac_plugin*)ud;
    strbuf* tagdata;
    reset_wrapper wrap;

    const tag* t;
    size_t i = 0;
    size_t m = 0;
    size_t total = 0;
    size_t len = 0;

    if(userdata->flag == 1) {
        wrap.userdata = userdata;
        wrap.dest = dest;
        r = userdata->psource.reset(userdata->psource.handle,&wrap,reset_cb);
        if(r != 0) return r;

        TRYS(stream_end(userdata));
        TRYS(stream_send(userdata,dest));

        miniogg_init(&userdata->ogg,userdata->ogg.serialno + 1);
        userdata->granulepos = 0;
        userdata->pts = 0 - userdata->padding;
        userdata->flag = 0;

        TRYS(stream_add_strbuf(userdata,&userdata->head,0));
        TRYS(stream_buffer(userdata));
    }

    tagdata = &userdata->tags;

    tagdata->len = userdata->tagpos + 4;

    m = taglist_len(tags);
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

        total++;
    }
    pack_u32be(&tagdata->x[0],(0x84 << 24) | (tagdata->len - 4));
    pack_u32le(&tagdata->x[userdata->tagpos],total);

    TRYS(stream_add_strbuf(userdata,&userdata->tags,0));
    TRYS(stream_buffer(userdata));

    userdata->flag = 1;

    (void)tags;
    (void)dest;

    r = 0;
    cleanup:
    return r;
}


static int plugin_config(void* ud, const strbuf* key, const strbuf* value) {
    (void)ud;
    (void)value;
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

static int plugin_get_caps(void* ud, packet_receiver_caps* caps) {
    (void)ud;
    caps->has_global_header = 1;
    return 0;
}

const muxer_plugin muxer_plugin_ogg_flac = {
    {.a = 0, .len = 8, .x = (uint8_t*)"ogg_flac" },
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define MINIOGG_IMPLEMENTATION
#include "miniogg.h"
#pragma GCC diagnostic pop

#define BASE64_ENCODE_IMPLEMENTATION
#include "base64encode.h"
