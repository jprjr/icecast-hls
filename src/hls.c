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
    m->disc = 0;
    m->init_id = 0;
}

void hls_segment_init(hls_segment* s) {
    strbuf_init(&s->expired_files);
    membuf_init(&s->data);
    s->samples = 0;
    s->init_id = 0;
    s->disc = 0;
}

void hls_segment_free(hls_segment* s) {
    strbuf_free(&s->data);
}

void hls_segment_reset(hls_segment* s) {
    strbuf_init(&s->expired_files);
    membuf_reset(&s->data);
    s->samples = 0;
    s->pts = 0;
    s->disc = 0;
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

int hls_playlist_open(hls_playlist* b, unsigned int segments) {
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

hls_segment_meta* hls_playlist_get(hls_playlist* b, size_t index) {
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
    strbuf_init(&h->init_format);
    strbuf_init(&h->init_filename);
    strbuf_init(&h->init_mimetype);
    strbuf_init(&h->segment_format);
    strbuf_init(&h->segment_mimetype);
    strbuf_init(&h->playlist_filename);
    strbuf_init(&h->playlist_mimetype);
    strbuf_init(&h->entry_prefix);
    hls_playlist_init(&h->playlist);
    hls_segment_init(&h->segment);
    h->callbacks.delete = hls_delete_default_callback;
    h->callbacks.write = hls_write_default_callback;
    h->callbacks.userdata = NULL;
    h->time_base = 0;
    h->target_samples = 0;
    h->target_duration = 2000;
    h->playlist_length = 60 * 15;
    h->media_sequence = 0;
    h->disc_sequence = 0;
    h->counter = 0;
    h->init_counter = 0;
    h->version = 7;
    h->now.seconds = 0;
    h->now.nanoseconds = 0;
}

void hls_free(hls* h) {
    strbuf_free(&h->txt);
    strbuf_free(&h->header);
    membuf_free(&h->segment.data);
    strbuf_free(&h->segment.expired_files);
    strbuf_free(&h->playlist_filename);
    strbuf_free(&h->playlist_mimetype);
    strbuf_free(&h->init_format);
    strbuf_free(&h->init_filename);
    strbuf_free(&h->init_mimetype);
    strbuf_free(&h->segment_format);
    strbuf_free(&h->segment_mimetype);
    strbuf_free(&h->entry_prefix);
    hls_playlist_free(&h->playlist);
    hls_init(h);
}

int hls_get_segment_info(const hls* h, const segment_source_info* info, segment_params* params) {
    params->segment_length = h->target_duration;
    if(info->frame_len != 0) params->packets_per_segment = (params->segment_length * info->time_base / info->frame_len / 1000);
    return 0;
}

int hls_open(hls* h, const segment_source* source) {
    int r;
    unsigned int playlist_segments;

    if(h->init_mimetype.len == 0) {
        /* if the source sets init_ext to NULL it's packed audio (no init segment) */
        if(source->init_mimetype != NULL) {
            TRYS(strbuf_copy(&h->init_mimetype,source->init_mimetype))
        }
    }

    if(h->init_format.len == 0) {
        /* if the source sets init_ext to NULL it's packed audio (no init segment) */
        if(source->init_ext != NULL) {
            TRYS(strbuf_sprintf(&h->init_format,"init-%%02u%.*s",
              (int)source->init_ext->len,(char *)source->init_ext->x))
            TRYS(strbuf_term(&h->init_format));
        }
    }

    if(h->playlist_filename.len == 0) {
        TRYS(strbuf_append_cstr(&h->playlist_filename,"stream.m3u8"));
    }

    if(h->playlist_mimetype.len == 0) {
        TRYS(strbuf_copy(&h->playlist_mimetype,&mime_m3u8));
    }

    if(h->segment_format.len == 0) {
      TRYS(strbuf_sprintf(&h->segment_format,"%%08u%.*s",
        (int)source->media_ext->len,(char *)source->media_ext->x))
      TRYS(strbuf_term(&h->segment_format));
    }

    if(h->segment_mimetype.len == 0) {
        TRYS(strbuf_copy(&h->segment_mimetype,source->media_mimetype));
    }

    h->target_samples = (h->target_duration * source->time_base / 1000);
    h->time_base  = source->time_base;

    playlist_segments = (h->playlist_length / (h->target_duration / 1000)) + 1;
    TRYS(hls_playlist_open(&h->playlist, playlist_segments))

    TRY0(strbuf_sprintf(&h->header,
      "#EXTM3U\n"
      "#EXT-X-VERSION:%u\n"
      "#EXT-X-TARGETDURATION:%u\n",
      h->version,
      (h->target_duration / 1000)),LOG0("out of memory"));

    r = 0;

    cleanup:
    return r;
}

static int hls_update_playlist(hls* h) {
    int r;
    size_t i;
    size_t len;
    size_t init_id;
    const hls_segment_meta* s;

    init_id = 0;

    strbuf_reset(&h->txt);
    len = hls_playlist_used(&h->playlist);

    TRYS(strbuf_cat(&h->txt,&h->header));

    TRYS(strbuf_sprintf(&h->txt,
      "#EXT-X-MEDIA-SEQUENCE:%u\n"
      "#EXT-X-DISCONTINUITY-SEQUENCE:%u\n",
      h->media_sequence,
      h->disc_sequence));

    for(i=0;i<len;i++) {
        s = hls_playlist_get(&h->playlist,i);

        if(s->disc) {
            TRYS(strbuf_sprintf(&h->txt,
              "#EXT-X-DISCONTINUITY\n"));
        }

        if(s->init_id != init_id) {

            h->init_filename.len = 0;
            TRYS(strbuf_sprintf(&h->init_filename,(char*)h->init_format.x,s->init_id));

            TRYS(strbuf_sprintf(&h->txt,
                "#EXT-X-MAP:URI=\"%.*s%.*s\"\n",
                (int)h->entry_prefix.len,
                (const char*)h->entry_prefix.x,
                (int)h->init_filename.len,
                (const char*)h->init_filename.x));
            init_id = s->init_id;
        }

        TRYS(strbuf_cat(&h->txt,&s->tags))
    }

    cleanup:
    return r;
}

static int hls_flush_segment(hls* h) {
    int r;
    hls_segment_meta* t;
    hls_segment_meta* oldt;
    ich_tm tm;
    ich_frac f;
    strbuf stmp = STRBUF_ZERO;

    size_t len;

    if(hls_playlist_isfull(&h->playlist)) {
        oldt = hls_playlist_shift(&h->playlist);
        h->callbacks.delete(h->callbacks.userdata, &oldt->filename);
        h->media_sequence++;
        if(oldt->disc) h->disc_sequence++;
        if(oldt->expired_files.len > 0) {
            stmp.x = (uint8_t*)oldt->expired_files.x;
            len = oldt->expired_files.len;
            while(len) {
                stmp.len = strlen((char*)stmp.x);
                h->callbacks.delete(h->callbacks.userdata,&stmp);
                stmp.x += stmp.len + 1;
                len    -= stmp.len + 1;
            }
            strbuf_free(&oldt->expired_files);
        }
        if( (t = hls_playlist_get(&h->playlist, 0)) != NULL) {
            if(t->init_id != oldt->init_id) { /* not using this init segment anymore */
                h->init_filename.len = 0;
                TRYS(strbuf_sprintf(&h->init_filename,(char *)h->init_format.x,oldt->init_id));
                h->callbacks.delete(h->callbacks.userdata,&h->init_filename);
            }
        }
    }

    t = hls_playlist_push(&h->playlist);
    hls_segment_meta_reset(t);

    t->expired_files = h->segment.expired_files;
    t->disc    = h->segment.disc;
    t->init_id = h->segment.init_id;

    TRYS(strbuf_sprintf(&t->filename,(char*)h->segment_format.x,++(h->counter)));

    ich_time_to_tm(&tm,&h->now);
    TRYS(strbuf_sprintf(&t->tags,
      "#EXT-X-PROGRAM-DATE-TIME:%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\n",
      tm.year,tm.month,tm.day,tm.hour,tm.min,tm.sec,tm.mill));

    TRYS(strbuf_sprintf(&t->tags,
      "#EXTINF:%f,\n"
      "%.*s%.*s\n",
      (((double)h->segment.samples) / ((double)h->time_base)),
      (int)h->entry_prefix.len, (const char*)h->entry_prefix.x,
      (int)t->filename.len, (const char*)t->filename.x));

    TRY0(h->callbacks.write(h->callbacks.userdata, &t->filename, &h->segment.data, &h->segment_mimetype),
      LOGS("error writing file %.*s", t->filename));

    f.num = h->segment.samples;
    f.den = h->time_base;
    ich_time_add_frac(&h->now,&f);

    hls_segment_reset(&h->segment);

    TRYS(hls_update_playlist(h));

    cleanup:
    return r;
}

static int hls_write_playlist(hls* h) {
    int r;

    TRY0(h->callbacks.write(h->callbacks.userdata,&h->playlist_filename,&h->txt,&h->playlist_mimetype),LOGS("error writing file %.*s",h->playlist_filename));

    cleanup:
    return r;
}


int hls_add_segment(hls* h, const segment* s) {
    int r;
    membuf tmp = MEMBUF_ZERO;

    if(s->type == SEGMENT_TYPE_INIT) {
        if(h->segment.samples > 0) { /* force a flush */
            TRY0(hls_flush_segment(h),LOG0("error flushing segment"));
            TRY0(hls_write_playlist(h),LOG0("error writing playlist"));
        }
        tmp.x = (void*)s->data;
        tmp.len = s->len;
        h->init_filename.len = 0;
        TRYS(strbuf_sprintf(&h->init_filename,(char*)h->init_format.x,++(h->init_counter)));
        TRY0(h->callbacks.write(h->callbacks.userdata,&h->init_filename,&tmp,&h->init_mimetype),
          LOGS("error writing file %.*s", h->init_filename));
        h->segment.init_id = h->init_counter;
        return 0;
    }

    if(h->segment.samples + s->samples > h->target_samples) { /* time to flush! */
        TRY0(hls_flush_segment(h),LOG0("error flushing segment"));
        TRY0(hls_write_playlist(h),LOG0("error writing playlist"));
    }

    TRYS(membuf_append(&h->segment.data,s->data,s->len));
    if(h->segment.samples == 0) h->segment.pts = s->pts;
    h->segment.samples += s->samples;

    cleanup:
    return r;

}

int hls_flush(hls* h) {
    int r;

    if(h->segment.samples != 0) {
        TRY0(hls_flush_segment(h),LOG0("hls_flush: error flushing segment"));
    }

    TRYS(strbuf_append_cstr(&h->txt,"#EXT-X-ENDLIST\n"));
    TRY0(hls_write_playlist(h),LOG0("error writing playlist"));

    cleanup:
    return r;
}

int hls_reset(hls* h) {
    int r;

    if(h->segment.samples != 0) {
        TRY0(hls_flush_segment(h),LOG0("hls_flush: error flushing segment"));
        TRY0(hls_write_playlist(h),LOG0("error writing playlist"));
    }

    h->segment.disc = 1;

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
        h->target_duration *= 1000;
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

    if(strbuf_ends_cstr(key,"init-format")) {
        TRYS(strbuf_copy(&h->init_format,value));
        TRYS(strbuf_term(&h->segment_format));
        return 0;
    }

    if(strbuf_ends_cstr(key,"init-mimetype")) {
        TRYS(strbuf_copy(&h->init_mimetype,value));
        return 0;
    }

    if(strbuf_ends_cstr(key,"playlist-filename")) {
        TRYS(strbuf_copy(&h->playlist_filename,value));
        return 0;
    }

    if(strbuf_ends_cstr(key,"playlist-mimetype")) {
        TRYS(strbuf_copy(&h->playlist_mimetype,value));
        return 0;
    }

    if(strbuf_ends_cstr(key,"entry-prefix")) {
        TRYS(strbuf_copy(&h->entry_prefix,value));
        return 0;
    }

    if(strbuf_ends_cstr(key,"segment-format")) {
        TRYS(strbuf_copy(&h->segment_format,value));
        TRYS(strbuf_term(&h->segment_format));
        return 0;
    }

    if(strbuf_ends_cstr(key,"segment-mimetype")) {
        TRYS(strbuf_copy(&h->segment_mimetype,value));
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

    TRYS(strbuf_append(&out->mime,"-->",3));
    if(src->desc.len > 0) TRYS(strbuf_copy(&out->desc,&src->desc));
    strbuf_reset(&out->data);
    if(h->entry_prefix.len != 0) TRYS(strbuf_cat(&out->data,&h->entry_prefix));
    TRYS(strbuf_cat(&out->data,&dest_filename));
    r = 0;

    cleanup:
    strbuf_free(&dest_filename);
    return r;
}

int hls_expire_file(hls* h, const strbuf* filename) {
    int r;

    TRYS(strbuf_cat(&h->segment.expired_files,filename));
    TRYS(strbuf_term(&h->segment.expired_files));

    cleanup:
    return r;
}

