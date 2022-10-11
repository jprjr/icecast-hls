#include "hls.h"
#include "thread.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define OUTOFMEM "out of memory"

#define LOG0(fmt)     fprintf(stderr,"[hls] "fmt"\n")
#define LOG1(fmt,a)   fprintf(stderr,"[hls] "fmt"\n",(a))
#define LOG2(fmt,a,b) fprintf(stderr,"[hls] "fmt"\n",(a),(b))
#define LOG3(fmt,a,b,c) fprintf(stderr,"[hls] "fmt"\n",(a),(b),(c))
#define LOGS(fmt,s) LOG2(fmt, (int)s.len, (char *)s.x)

#define TRY0(exp, act) if( (r = (exp)) != 0 ) { act; goto cleanup; }
#define TRYS(exp) TRY0(exp, LOG0(OUTOFMEM); abort())

/* keep a global counter for writing out picture files. We could
 * have multiple file destinations going to the same folder,
 * this ensures multiple threads don't stop on each other */

static thread_atomic_uint_t counter;

static STRBUF_CONST(mime_m3u8,"application/vnd.apple.mpegurl");

static int hls_delete_default_callback(void* userdata, const strbuf* filename) {
    (void)userdata;
    (void)filename;
    LOG0("delete callback not set"); abort();
    return -1;
}

static int hls_write_default_callback(void* userdata, const strbuf* filename, const membuf* data, const strbuf* mime) {
    (void)userdata;
    (void)filename;
    (void)data;
    (void)mime;
    LOG0("write callback not set"); abort();
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
    strbuf_init(&s->expired_files);
    membuf_init(&s->data);
    s->samples = 0;
}

void hls_segment_free(hls_segment* s) {
    strbuf_free(&s->data);
}

void hls_segment_reset(hls_segment* s) {
    strbuf_init(&s->expired_files);
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
    thread_atomic_uint_store(&counter,0);
    strbuf_init(&h->txt);
    strbuf_init(&h->header);
    strbuf_init(&h->fmt);
    strbuf_init(&h->init_filename);
    strbuf_init(&h->playlist_filename);
    strbuf_init(&h->init_mime);
    strbuf_init(&h->media_ext);
    strbuf_init(&h->media_mime);
    strbuf_init(&h->entry_prefix);
    hls_playlist_init(&h->playlist);
    hls_segment_init(&h->segment);
    h->callbacks.delete = hls_delete_default_callback;
    h->callbacks.write = hls_write_default_callback;
    h->callbacks.userdata = NULL;
    h->time_base = 0;
    h->target_samples = 0;
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
    strbuf_free(&h->segment.expired_files);
    strbuf_free(&h->init_filename);
    strbuf_free(&h->playlist_filename);
    strbuf_free(&h->init_mime);
    strbuf_free(&h->media_ext);
    strbuf_free(&h->media_mime);
    strbuf_free(&h->entry_prefix);
    hls_playlist_free(&h->playlist);
    hls_init(h);
}

int hls_open(hls* h, const segment_source* source) {
    int r;
    unsigned int playlist_segments;
    size_t packets_per_segment;
    segment_source_params params = SEGMENT_SOURCE_PARAMS_ZERO;

    h->time_base  = source->time_base;
    packets_per_segment = (h->target_duration * source->time_base / source->frame_len) + (source->time_base % source->frame_len > (source->frame_len / 2));
    h->target_samples = packets_per_segment * source->frame_len;

    if(source->media_mime != NULL) {
        TRYS(strbuf_copy(&h->media_mime,source->media_mime));
    }

    if(source->media_ext != NULL) {
        TRYS(strbuf_copy(&h->media_ext,source->media_ext))
    }

    if(source->init_mime != NULL) {
        TRYS(strbuf_copy(&h->init_mime,source->init_mime))
    }

    if(source->init_ext != NULL) {

        if(h->init_filename.len == 0) {
            TRYS(strbuf_append_cstr(&h->init_filename,"init"))
        }

        TRYS(strbuf_cat(&h->init_filename,source->init_ext))
    }

    if(h->playlist_filename.len == 0) {
        TRYS(strbuf_append_cstr(&h->playlist_filename,"stream.m3u8"));
    }

    playlist_segments = (h->playlist_length / h->target_duration) + (source->time_base % source->frame_len <= (source->frame_len / 2));
    TRYS(hls_playlist_open(&h->playlist, playlist_segments))

    TRY0(strbuf_sprintf(&h->header,
      "#EXTM3U\n"
      "#EXT-X-TARGETDURATION:%u\n"
      "#EXT-X-VERSION:%u\n",
      h->target_duration,
      h->version),LOG0("out of memory"));

    TRYS(strbuf_sprintf(&h->fmt,"%%08u%.*s",
      (int)h->media_ext.len,(char *)h->media_ext.x))
    TRYS(strbuf_term(&h->fmt));

    params.segment_length = h->target_duration;

    r = source->set_params(source->handle, &params);

    cleanup:
    return r;
}

static int hls_update_playlist(hls* h) {
    int r;
    size_t i;
    size_t len;
    hls_segment_meta* s;

    strbuf_reset(&h->txt);
    len = hls_playlist_used(&h->playlist);

    TRYS(strbuf_cat(&h->txt,&h->header));
    TRYS(strbuf_sprintf(&h->txt,
      "#EXT-X-MEDIA_SEQUENCE:%u\n\n",
      h->media_sequence))

    for(i=0;i<len;i++) {
        s = hls_playlist_get(&h->playlist,i);
        TRYS(strbuf_cat(&h->txt,&s->tags))
    }

    cleanup:
    return r;
}

static int hls_flush_segment(hls* h) {
    int r;
    hls_segment_meta* t;
    ich_tm tm;
    ich_frac f;
    strbuf stmp = STRBUF_ZERO;

    size_t len;

    if(hls_playlist_isfull(&h->playlist)) {
        t = hls_playlist_shift(&h->playlist);
        h->callbacks.delete(h->callbacks.userdata, &t->filename);
        h->media_sequence++;
        if(t->expired_files.len > 0) {
            stmp.x = (uint8_t*)t->expired_files.x;
            len = t->expired_files.len;
            while(len) {
                stmp.len = strlen((char*)stmp.x);
                h->callbacks.delete(h->callbacks.userdata,&stmp);
                stmp.x += stmp.len + 1;
                len    -= stmp.len + 1;
            }
            strbuf_free(&t->expired_files);
        }
    }

    t = hls_playlist_push(&h->playlist);
    hls_segment_meta_reset(t);

    t->expired_files = h->segment.expired_files;
    TRYS(strbuf_sprintf(&t->filename,(char*)h->fmt.x,++(h->counter)));

    ich_time_to_tm(&tm,&h->now);
    TRYS(strbuf_sprintf(&t->tags,
      "#EXT-X-PROGRAM-DATE-TIME:%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\n"
      "#EXTINF:%f,\n"
      "%.*s%.*s\n\n",
      tm.year,tm.month,tm.day,tm.hour,tm.min,tm.sec,tm.mill,
      (((double)h->segment.samples) / ((double)h->time_base)),
      (int)h->entry_prefix.len, (const char*)h->entry_prefix.x,
      (int)t->filename.len, (const char*)t->filename.x));

    TRY0(h->callbacks.write(h->callbacks.userdata, &t->filename, &h->segment.data, &h->media_mime),
      LOGS("error writing file %.*s", t->filename));

    f.num = h->segment.samples;
    f.den = h->time_base;
    ich_time_add_frac(&h->now,&f);

    hls_segment_reset(&h->segment);

    TRYS(hls_update_playlist(h));

    cleanup:
    return r;
}


int hls_add_segment(hls* h, const segment* s) {
    int r;
    membuf tmp = MEMBUF_ZERO;

    if(s->type == SEGMENT_TYPE_INIT) {
        TRYS(strbuf_sprintf(&h->header,
          "#EXT-X-MAP:URI=\"%.*s%.*s\"\n",
          (int)h->entry_prefix.len,
          (const char*)h->entry_prefix.x,
          (int)h->init_filename.len,
          (const char*)h->init_filename.x));
        tmp.x = (void*)s->data;
        tmp.len = s->len;
        return h->callbacks.write(h->callbacks.userdata,&h->init_filename,&tmp,&h->init_mime);
    }

    TRYS(membuf_append(&h->segment.data,s->data,s->len));
    h->segment.samples += s->samples;

    if(h->segment.samples >= h->target_samples) { /* time to flush! */
        TRY0(hls_flush_segment(h),LOG0("error flushing segment"));
        TRY0(h->callbacks.write(h->callbacks.userdata,&h->playlist_filename,&h->txt,&mime_m3u8),LOGS("error writing file %.*s",h->playlist_filename));
    }

    cleanup:
    return r;

}

int hls_flush(hls* h) {
    int r;

    if(h->segment.samples != 0) {
        TRY0(hls_flush_segment(h),LOG0("hls_flush: error flushing segment"));
    }

    TRYS(strbuf_append_cstr(&h->txt,"#EXT-X-ENDLIST\n"));

    r = h->callbacks.write(h->callbacks.userdata,&h->playlist_filename,&h->txt,&mime_m3u8);

    cleanup:
    return r;
}

const strbuf* hls_get_playlist(const hls* h) {
    return &h->txt;
}

int hls_configure(hls* h, const strbuf* key, const strbuf* value) {
    int r = -1;

    if(strbuf_ends_cstr(key,"target-duration")) {
        errno = 0;
        h->target_duration = strbuf_strtoul(value,10);
        if(errno != 0) {
            LOGS("error parsing target-duration value %.*s",(*value));
            return -1;
        }
        if(h->target_duration == 0) {
            LOGS("invalid target-duration %.*s",(*value));
            return -1;
        }
        return 0;
    }

    if(strbuf_ends_cstr(key,"playlist-length")) {
        errno = 0;
        h->playlist_length = strbuf_strtoul(value,10);
        if(errno != 0) {
            LOGS("error parsing playlist-length value %.*s",(*value));
            return -1;
        }
        if(h->playlist_length == 0) {
            LOGS("invalid playlist-length %.*s",(*value));
            return -1;
        }
        return 0;
    }

    if(strbuf_ends_cstr(key,"init-basename")) {
        TRYS(strbuf_copy(&h->init_filename,value));
        return 0;
    }

    if(strbuf_ends_cstr(key,"playlist-filename")) {
        TRYS(strbuf_copy(&h->playlist_filename,value));
        return 0;
    }

    if(strbuf_ends_cstr(key,"entry-prefix")) {
        TRYS(strbuf_copy(&h->entry_prefix,value));
        return 0;
    }

    LOGS("unknown key %.*s", (*key));

    cleanup:
    return r;
}

/* used by the outputs when we're trying to move a picture out-of-band,
 * write the picture out via callback, return picture URL in out */
int hls_submit_picture(hls* h, const picture* src, picture* out) {
    int r;
    strbuf dest_filename = STRBUF_ZERO;
    strbuf mime = STRBUF_ZERO;
    int picture_id;
    const char *fmt_str;

    picture_id = thread_atomic_uint_inc(&counter) % 100000000;

    if(strbuf_ends_cstr(&src->mime,"/png")) {
        fmt_str = "%08u.png";
        mime = src->mime;
    } else if(strbuf_ends_cstr(&src->mime,"/jpg") || strbuf_ends_cstr(&src->mime,"jpeg")) {
        fmt_str = "%08u.jpg";
        mime = src->mime;
    } else if(strbuf_ends_cstr(&src->mime,"/gif")) {
        fmt_str = "%08u.gif";
        mime = src->mime;
    } else if(strbuf_ends_cstr(&src->mime,"/webp")) {
        fmt_str = "%08u.webp";
        mime = src->mime;
    } else if(strbuf_equals_cstr(&src->mime,"image/")) {
        fmt_str = "%08u.jpg";
        mime.x = (uint8_t*)"image/jpg";
        mime.len = 9;
    } else {
        /* we just return 0 and clean up so the caller
         * just strips the image */
        LOGS("WARNING: unknown image mime type %.*s",src->mime);
        r = 0; goto cleanup;
    }

    TRYS(strbuf_sprintf(&dest_filename,fmt_str,picture_id));
    TRYS(h->callbacks.write(h->callbacks.userdata, &dest_filename, &src->data, &mime));

    LOG3("segment %lu: wrote picture %.*s",
      h->counter+1,(int)dest_filename.len,(char *)dest_filename.x);

    TRYS(strbuf_append(&out->mime,"-->",3));
    if(src->desc.len > 0) TRYS(strbuf_copy(&out->desc,&src->desc));
    TRYS(strbuf_copy(&out->data,&dest_filename));
    r = 0;

    cleanup:
    strbuf_free(&dest_filename);
    return r;
}

int hls_expire_file(hls* h, const strbuf* filename) {
    int r;

    TRYS(strbuf_cat(&h->segment.expired_files,filename));
    TRYS(strbuf_term(&h->segment.expired_files));
    LOG3("segment %lu: marking file as expired %.*s",
      h->counter+1,(int)filename->len,(char *)filename->x);

    cleanup:
    return r;
}

