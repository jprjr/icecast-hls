#include "muxer_plugin_ogg_opus.h"

#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#include "pack_u32le.h"
#include "pack_u16le.h"

#define MINIOGG_API static
#include "miniogg.h"

#include "base64encode.h"

#define LOG0(s) fprintf(stderr,"[muxer:ogg:opus] "s"\n")
#define LOG1(s, a) fprintf(stderr,"[muxer:ogg:opus] "s"\n", (a))
#define LOG2(s, a, b) fprintf(stderr,"[muxer:ogg:opus] "s"\n", (a), (b))
#define LOGS(s, a) LOG2(s, (int)(a).len, (const char *)(a).x )

#define LOGERRNO(s) LOG1(s": %s", strerror(errno))

#define TRYNULL(exp, act) if( (exp) == NULL) { act; r=-1; goto cleanup; }
#define TRY(exp, act) if(!(exp)) { act; r=-1; goto cleanup ; }
#define TRY0(exp, act) if( (r = (exp)) != 0 ) { act; goto cleanup; }
#define TRYS(exp) TRY0(exp, LOG0("out of memory"); abort())

#define USE_KEYFRAMES 1

static STRBUF_CONST(mime_ogg,"application/ogg");
static STRBUF_CONST(ext_ogg,".ogg");

struct ogg_opus_stream {
    strbuf head;
    strbuf tags;
    uint32_t tagpos;
    strbuf segment;
    miniogg ogg;
    uint64_t pts;
    uint64_t granulepos;
    uint64_t samples;
};

typedef struct ogg_opus_stream ogg_opus_stream;

struct ogg_opus_plugin {
    unsigned int padding;
    unsigned int keyframes;
    packet_source psource;
    ogg_opus_stream streams[2];
    uint8_t cur_stream;
    uint64_t samples_per_segment;
    segment_source_params segment_params;
    strbuf scratch;
};

typedef struct ogg_opus_plugin ogg_opus_plugin;

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

static int stream_send(ogg_opus_stream* stream, const segment_receiver* dest) {
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

static int stream_end(ogg_opus_stream* stream) {
    int r = -1;

    miniogg_eos(&stream->ogg);
    TRYS(membuf_append(&stream->segment,stream->ogg.header,stream->ogg.header_len));
    TRYS(membuf_append(&stream->segment,stream->ogg.body,stream->ogg.body_len));

    r = 0;
    cleanup:
    return r;
}

static int stream_buffer(ogg_opus_stream* stream) {
    int r = -1;

    miniogg_finish_page(&stream->ogg);
    TRYS(membuf_append(&stream->segment,stream->ogg.header,stream->ogg.header_len));
    TRYS(membuf_append(&stream->segment,stream->ogg.body,stream->ogg.body_len));

    r = 0;
    cleanup:
    return r;
}

static int stream_add_strbuf(ogg_opus_stream* stream, const strbuf* data,uint64_t granulepos) {
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
    ogg_opus_plugin *userdata = (ogg_opus_plugin*)malloc(sizeof(ogg_opus_plugin));
    if(userdata == NULL) return NULL;
    unsigned int i;

    userdata->keyframes = 0;
    userdata->psource = packet_source_zero;
    userdata->cur_stream = 2;
    userdata->segment_params = segment_source_params_zero;
    userdata->samples_per_segment = 0;
    strbuf_init(&userdata->scratch);

    for(i=0;i<2;i++) {
        strbuf_init(&userdata->streams[i].head);
        strbuf_init(&userdata->streams[i].tags);
        strbuf_init(&userdata->streams[i].segment);
        userdata->streams[i].tagpos = 0;
        userdata->streams[i].granulepos = 0;
        userdata->streams[i].samples = 0;
        userdata->streams[i].pts = 0;
    }
    miniogg_init(&userdata->streams[0].ogg,(uint32_t)rand());
    return userdata;
}

static void plugin_close(void* ud) {
    ogg_opus_plugin* userdata = (ogg_opus_plugin*)ud;
    unsigned int i;

    for(i=0;i<2;i++) {
        strbuf_free(&userdata->streams[i].head);
        strbuf_free(&userdata->streams[i].tags);
        strbuf_free(&userdata->streams[i].segment);
    }
    strbuf_free(&userdata->scratch);
    free(userdata);
}

static int receive_params(void* ud, const segment_source_params* params) {
    ogg_opus_plugin* userdata = (ogg_opus_plugin*)ud;
    userdata->segment_params = *params;
    return 0;
}

static int plugin_open(void* ud, const packet_source* source, const segment_receiver* dest) {
    int r = -1;
    uint64_t samples_per_segment = 0;
    unsigned int i = 0;
    ogg_opus_plugin* userdata = (ogg_opus_plugin*)ud;
    ogg_opus_stream* stream = NULL;

    /* prep the tag buffers */
    for(i=0;i<2;i++) {
        stream = &userdata->streams[i];
        TRYS(strbuf_append_cstr(&stream->tags,"OpusTags"));
        if(source->name == NULL) {
            TRYS(ogg_pack_cstr(&stream->tags,"icecast-hls"));
        } else {
            TRYS(ogg_pack_str(&stream->tags,(const char *)source->name->x,source->name->len));
        }
        TRYS(strbuf_readyplus(&stream->tags,4));
        stream->tagpos = stream->tags.len;
        stream->tags.len += 4;
        pack_u32le(&stream->tags.x[stream->tagpos],0);
    }
    userdata->padding = source->padding;
    userdata->streams[0].pts = 0 - source->padding;

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

static int plugin_submit_dsi(void* ud, const strbuf* data, const segment_receiver* dest) {
    int r = -1;
    unsigned int i = 0;

    ogg_opus_plugin* userdata = (ogg_opus_plugin*)ud;
    ogg_opus_stream* pri;

    for(i=0;i<2;i++) {
        TRYS(strbuf_copy(&userdata->streams[i].head,data));
    }

    pri = &userdata->streams[0];

    TRYS(stream_add_strbuf(pri,&pri->head,0));
    TRYS(stream_buffer(pri));

    r = 0;
    cleanup:
    (void)dest;
    return 0;
}

static int plugin_submit_packet(void* ud, const packet* p, const segment_receiver* dest) {
    int r = -1;

    ogg_opus_plugin* userdata = (ogg_opus_plugin*)ud;

    ogg_opus_stream* pri;
    ogg_opus_stream* alt;

    if(userdata->cur_stream == 2) {
        /* getting a packet before tags, add our blank opustags block */
        pri = &userdata->streams[0];
        TRYS(stream_add_strbuf(pri,&pri->tags,0));
        TRYS(stream_buffer(pri));
        userdata->cur_stream = 0;
    }

    pri = &userdata->streams[userdata->cur_stream];
    alt = &userdata->streams[!userdata->cur_stream];

    pri->granulepos += p->duration;
    TRYS(stream_add_strbuf(pri,&p->data,pri->granulepos));
    pri->samples += p->duration;

    if(pri->samples >= userdata->samples_per_segment) {
        TRYS(stream_buffer(pri));
        TRYS(stream_send(pri,dest));
    }

    if(userdata->keyframes) {
        alt->granulepos += p->duration;
        TRYS(stream_add_strbuf(alt,&p->data,alt->granulepos));
        alt->samples += p->duration;
        userdata->keyframes--;
        if(userdata->keyframes == 0) {
            TRYS(stream_end(pri));
            TRYS(stream_send(pri,dest));

            userdata->cur_stream = !userdata->cur_stream;
        }
    }

    r = 0;
    cleanup:
    return r;
}

static int plugin_flush(void* ud, const segment_receiver* dest) {
    int r = -1;

    ogg_opus_plugin* userdata = (ogg_opus_plugin*)ud;

    ogg_opus_stream* pri;

    if(userdata->cur_stream == 2) {
        /* getting a flush before we ever got tags / sent anything, just close */
        return dest->flush(dest->handle);
    }

    pri = &userdata->streams[userdata->cur_stream];

    TRYS(stream_end(pri));
    TRYS(stream_send(pri,dest));

    r = dest->flush(dest->handle);
    cleanup:
    return r;
}

typedef struct reset_wrapper {
    ogg_opus_plugin* userdata;
    const segment_receiver* dest;
} reset_wrapper;

static int reset_cb(void* ud, const packet* p) {
    reset_wrapper* wrap = (reset_wrapper*)ud;
    return plugin_submit_packet(wrap->userdata, p, wrap->dest);
}

static int plugin_submit_tags(void* ud, const taglist* tags, const segment_receiver* dest) {
    int r = -1;
    uint8_t next;
    ogg_opus_plugin* userdata = (ogg_opus_plugin*)ud;
    ogg_opus_stream* stream;
    ogg_opus_stream* prev;
    strbuf* tagdata;
    reset_wrapper wrap;

    const tag* t;
    size_t i = 0;
    size_t m = 0;
    size_t total = 0;
    size_t len = 0;

    if(userdata->cur_stream == 2) {/* seeing tags before we've gotten any packets */
        next = 0;
        userdata->cur_stream = 0;
    } else {
        /* we're seeing tags sometime after sending actual data, time to perform
         * the stream switch */
#if USE_KEYFRAMES
        r = userdata->psource.set_keyframes(userdata->psource.handle,4);
        if(r != 0) {
#endif
            /* packet source does not support setting keyframes, we'll reset the encoder */
            wrap.userdata = userdata;
            wrap.dest = dest;
            r = userdata->psource.reset(userdata->psource.handle,&wrap,reset_cb);
            if(r != 0) return r;

            stream = &userdata->streams[userdata->cur_stream];

            TRYS(stream_end(stream));
            TRYS(stream_send(stream,dest));

            miniogg_init(&stream->ogg,stream->ogg.serialno + 1);
            stream->granulepos = 0;
            stream->pts = 0 - userdata->padding;

            TRYS(stream_add_strbuf(stream,&stream->head,0));
            TRYS(stream_buffer(stream));
            next = userdata->cur_stream;
            goto add_tags;
#if USE_KEYFRAMES
        }
#endif
        next = !userdata->cur_stream;
        userdata->keyframes = 4;
        stream = &userdata->streams[next];
        prev = &userdata->streams[!next];

        miniogg_init(&stream->ogg,prev->ogg.serialno + 1);

        /* set the next stream's pre-skip to our number of keyframes */
        pack_u16le(&stream->head.x[10], userdata->psource.frame_len * userdata->keyframes);
        stream->pts = 0 - (userdata->psource.frame_len * userdata->keyframes);
        stream->granulepos = 0;
        TRYS(stream_add_strbuf(stream,&stream->head,0));
        TRYS(stream_buffer(stream));
    }

    add_tags:
    stream = &userdata->streams[next];
    tagdata = &stream->tags;

    tagdata->len = stream->tagpos + 4;

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
    pack_u32le(&tagdata->x[stream->tagpos],total);

    TRYS(stream_add_strbuf(stream,&stream->tags,0));
    TRYS(stream_buffer(stream));

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

const muxer_plugin muxer_plugin_ogg_opus = {
    {.a = 0, .len = 8, .x = (uint8_t*)"ogg_opus" },
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
