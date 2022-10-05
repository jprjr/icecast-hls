#include "hls.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define LOG0(fmt)     fprintf(stderr,"[hls] "fmt"\n")
#define LOG1(fmt,a)   fprintf(stderr,"[hls] "fmt"\n",(a))
#define LOG2(fmt,a,b) fprintf(stderr,"[hls] "fmt"\n",(a),(b))
#define LOGS(fmt,s) LOG2(fmt, ((int)(s)->len), ((const char*)(s)->x) )

static STRBUF_CONST(mime_m3u8,"application/iso-whatever-playlisything");
static STRBUF_CONST(filename_m3u8,"stream.m3u8");

static int hls_delete_default_callback(void* userdata, const strbuf* filename) {
    (void)userdata;
    (void)filename;
    LOG0("delete callback not set");
    return -1;
}

static int hls_write_default_callback(void* userdata, const strbuf* filename, const membuf* data, const strbuf* mime) {
    (void)userdata;
    (void)filename;
    (void)data;
    (void)mime;
    LOG0("write callback not set");
    return -1;
}

void hls_segment_meta_init(hls_segment_meta* m) {
    strbuf_init(&m->filename);
    strbuf_init(&m->tags);
}

void hls_segment_meta_free(hls_segment_meta *m) {
    strbuf_free(&m->filename);
    strbuf_free(&m->tags);
}

void hls_segment_meta_reset(hls_segment_meta *m) {
    strbuf_reset(&m->filename);
    strbuf_reset(&m->tags);
}

void hls_segment_init(hls_segment* s) {
    membuf_init(&s->data);
    s->samples = 0;
}

void hls_segment_free(hls_segment* s) {
    strbuf_free(&s->data);
}

void hls_segment_reset(hls_segment* s) {
    membuf_reset(&s->data);
    s->samples = 0;
}

void hls_playlist_init(hls_playlist* p) {
    p->segments = NULL;
    p->size = 0;
    p->head = 0;
    p->tail = 0;
}

void hls_playlist_free(hls_playlist* p) {
    size_t i = 0;
    if(p->size != 0) {
        for(i=0;i<p->size;i++) {
            hls_segment_meta_free(&p->segments[i]);
        }
        free(p->segments);
    }
    hls_playlist_init(p);
}

int hls_playlist_open(hls_playlist* b, size_t segments) {
    size_t i = 0;
    b->size = segments + 1;
    b->segments = (hls_segment_meta*)malloc(sizeof(hls_segment_meta) * b->size);
    if(b->segments == NULL) {
        b->size = 0;
        return -1;
    }
    for(i=0;i<b->size;i++) {
        hls_segment_meta_init(&b->segments[i]);
    }
    return 0;
}

int hls_playlist_isempty(const hls_playlist* b) {
    return b->head == b->tail;
}

int hls_playlist_isfull(const hls_playlist* b) {
    size_t head = b->head+1;
    if(head == b->size) head = 0;
    return head == b->tail;
}

size_t hls_playlist_avail(const hls_playlist* b) {
    if(b->head >= b->tail) {
        return (b->size-1) - (b->head - b->tail);
    }
    return b->tail - b->head - 1;
}

size_t hls_playlist_used(const hls_playlist* b) {
    return (b->size-1) - hls_playlist_avail(b);
}

hls_segment_meta* hls_playlist_get(hls_playlist* b,size_t index) {
    index += b->tail;
    if(index >= b->size) index -= b->size;
    return &b->segments[index];
}

hls_segment_meta* hls_playlist_push(hls_playlist* p) {
    hls_segment_meta* t;

    t = &p->segments[p->head];
    p->head++;
    if(p->head == p->size) {
        p->head = 0;
    }
    return t;
}

hls_segment_meta* hls_playlist_shift(hls_playlist* b) {
    hls_segment_meta* s = NULL;
    if(b->head != b->tail) {
        s = &b->segments[b->tail];
        b->tail++;
        if(b->tail == b->size) b->tail = 0;
    }
    return s;
}


void hls_init(hls* h) {
    strbuf_init(&h->txt);
    strbuf_init(&h->header);
    strbuf_init(&h->fmt);
    strbuf_init(&h->init_filename);
    strbuf_init(&h->init_mime);
    strbuf_init(&h->media_ext);
    strbuf_init(&h->media_mime);
    hls_playlist_init(&h->playlist);
    hls_segment_init(&h->segment);
    h->callbacks.delete = hls_delete_default_callback;
    h->callbacks.write = hls_write_default_callback;
    h->callbacks.userdata = NULL;
    h->time_base = 0;
    h->target_duration = 2;
    h->playlist_length = 60 * 15;
    h->media_sequence = 1;
    h->counter = 0;
    h->version = 7;
    h->now.seconds = 0;
    h->now.nanoseconds = 0;
}

void hls_free(hls* h) {
    strbuf_free(&h->txt);
    strbuf_free(&h->header);
    strbuf_free(&h->fmt);
    membuf_free(&h->segment.data);
    hls_playlist_free(&h->playlist);
    hls_init(h);
}

#define TRY(x) if( (r = (x)) != 0 ) return r;

int hls_open(hls* h, const segment_source* source) {
    int r;
    unsigned int playlist_segments;
    segment_source_params params = SEGMENT_SOURCE_PARAMS_ZERO;

    h->time_base  = source->time_base;

    if(source->media_mime != NULL) {
        if(strbuf_copy(&h->media_mime,source->media_mime) != 0) return -1;
    }

    if(source->media_ext != NULL) {
        if(strbuf_copy(&h->media_ext,source->media_ext) != 0) return -1;
    }

    if(source->init_mime != NULL) {
        if(strbuf_copy(&h->init_mime,source->init_mime) != 0) return -1;
    }

    if(source->init_ext != NULL) {

        if(h->init_filename.len == 0) {
            if(strbuf_append_cstr(&h->init_filename,"init") != 0) {
                LOG0("out of memory");
                return -1;
            }
        }

        if(strbuf_cat(&h->init_filename,source->init_ext) != 0) {
            LOG0("out of memory");
            return -1;
        }
    }

    playlist_segments = (h->playlist_length / h->target_duration) + 1;
    TRY(hls_playlist_open(&h->playlist, playlist_segments))

    TRY(strbuf_sprintf(&h->header,
      "#EXTM3U\n"
      "#EXT-X-TARGETDURATION:%u\n"
      "#EXT-X-VERSION:%u\n",
      h->target_duration,
      h->version))

    TRY(strbuf_sprintf(&h->fmt,"%%08u%.*s",
      (int)h->media_ext.len,(char *)h->media_ext.x));
    TRY(strbuf_term(&h->fmt));

    params.segment_length = h->target_duration;

    return source->set_params(source->handle, &params);
}

static int hls_update_playlist(hls* h) {
    int r;
    size_t i;
    size_t len;
    hls_segment_meta* s;

    strbuf_reset(&h->txt);
    len = hls_playlist_used(&h->playlist);

    TRY(strbuf_cat(&h->txt,&h->header));
    TRY(strbuf_sprintf(&h->txt,
      "#EXT-X-MEDIA_SEQUENCE:%u\n\n",
      h->media_sequence))

    for(i=0;i<len;i++) {
        s = hls_playlist_get(&h->playlist,i);
        TRY(strbuf_cat(&h->txt,&s->tags))
    }
    return 0;
}

static int hls_flush_segment(hls* h) {
    int r;
    hls_segment_meta* t;
    ich_tm tm;
    ich_frac f;

    if(hls_playlist_isfull(&h->playlist)) {
        t = hls_playlist_shift(&h->playlist);
        h->callbacks.delete(h->callbacks.userdata, &t->filename);
        h->media_sequence++;
    }

    t = hls_playlist_push(&h->playlist);
    hls_segment_meta_reset(t);

    TRY(strbuf_sprintf(&t->filename,(char*)h->fmt.x,++(h->counter)));

    ich_time_to_tm(&tm,&h->now);
    TRY(strbuf_sprintf(&t->tags,
      "#EXT-X-PROGRAM-DATE-TIME:%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\n"
      "#EXTINF:%f,\n"
      "%.*s\n\n",
      tm.year,tm.month,tm.day,tm.hour,tm.min,tm.sec,tm.mill,
      (((double)h->segment.samples) / ((double)h->time_base)),
      (int)t->filename.len, (const char*)t->filename.x))

    TRY(h->callbacks.write(h->callbacks.userdata,&t->filename, &h->segment.data, &h->media_mime));

    f.num = h->segment.samples;
    f.den = h->time_base;
    ich_time_add_frac(&h->now,&f);

    hls_segment_reset(&h->segment);

    TRY(hls_update_playlist(h))
    return 0;
}


int hls_add_segment(hls* h, const segment* s) {
    int r;
    membuf tmp = MEMBUF_ZERO;

    if(s->type == SEGMENT_TYPE_INIT) {
        TRY(strbuf_sprintf(&h->header,
          "#EXT-X-MAP:URI=\"%.*s\"\n",
          (int)h->init_filename.len,
          (char*)h->init_filename.x));
        tmp.x = (void*)s->data;
        tmp.len = s->len;
        return h->callbacks.write(h->callbacks.userdata,&h->init_filename,&tmp,&h->init_mime);
    }

    if( (h->segment.samples + s->samples) / h->time_base >= h->target_duration) { /* time to flush! */
        TRY(hls_flush_segment(h));
        TRY(h->callbacks.write(h->callbacks.userdata,&filename_m3u8,&h->txt,&mime_m3u8));
    }

    TRY(membuf_append(&h->segment.data,s->data,s->len));
    h->segment.samples += s->samples;

    return 0;

}

int hls_flush(hls* h) {
    int r;

    if(h->segment.samples != 0) {
        TRY(hls_flush_segment(h));
    }

    TRY(strbuf_append_cstr(&h->txt,"#EXT-X-ENDLIST\n"));
    return h->callbacks.write(h->callbacks.userdata,&filename_m3u8,&h->txt,&mime_m3u8);
}

const strbuf* hls_get_playlist(const hls* h) {
    return &h->txt;
}

int hls_configure(hls* h, const strbuf* key, const strbuf* value) {
    if(strbuf_equals_cstr(key,"hls-target-duration")) {
        errno = 0;
        h->target_duration = strbuf_strtoul(value,10);
        if(errno != 0) {
            LOGS("error parsing target-duration value %.*s",value);
            return -1;
        }
        if(h->target_duration == 0) {
            fprintf(stderr,"[hls] invalid target-duration %.*s\n",
              (int)value->len,(char *)value->x);
            return -1;
        }
        return 0;
    }

    if(strbuf_equals_cstr(key,"hls-playlist-length")) {
        errno = 0;
        h->playlist_length = strbuf_strtoul(value,10);
        if(errno != 0) {
            LOGS("error parsing playlist-length value %.*s",value);
            return -1;
        }
        if(h->playlist_length == 0) {
            LOGS("invalid playlist-length %.*s",value);
            return -1;
        }
        return 0;
    }

    if(strbuf_equals_cstr(key,"hls-init-basename")) {
        if(strbuf_copy(&h->init_filename,value) != 0) {
            LOG0("out of memory");
            return -1;
        }
        return 0;
    }

    LOGS("unknown key %.*s", key);
    return -1;
}
