#include "frame.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* note - all frame operations need to work on the
 * membuf-allocated memory directly and not call
 * anything like, readyplus, membuf_append, etc */

void frame_init(frame* f) {
    membuf_init(&f->samples);
    f->channels = 0;
    f->duration = 0;
    f->format = SAMPLEFMT_UNKNOWN;
    f->sample_rate = 0;
    f->pts = 0;
}

void frame_free(frame* f) {
    size_t i;
    size_t len;
    membuf* m;

    m = (membuf*)f->samples.x;
    len = f->samples.len / sizeof(membuf);

    for(i=0;i<len;i++) {
        membuf_free(&m[i]);
    }
    membuf_free(&f->samples);

    frame_init(f);
}

static inline membuf* frame_get_channel_int(const frame* f, size_t idx) {
    idx *= sizeof(membuf);
    if(f->samples.len <= idx) return NULL;
    return (membuf*)&f->samples.x[idx];
}

membuf* frame_get_channel(const frame* f, size_t idx) {
    return frame_get_channel_int(f,idx);
}

void* frame_get_channel_samples(const frame* f, size_t idx) {
    membuf* m = frame_get_channel_int(f,idx);
    if(m == NULL) return NULL;
    return m->x;
}

int frame_ready(frame* f) {
    int r;
    size_t i;
    membuf m;
    membuf* mptr;

    if(f->channels == 0) return -1;

    if(samplefmt_is_planar(f->format)) {
        for(i=0;i<f->channels;i++) {
            if( (mptr = frame_get_channel_int(f,i)) == NULL) {
                membuf_init(&m);
                if( (r = membuf_append(&f->samples,&m,sizeof(membuf))) != 0) {
                    fprintf(stderr,"out of memory\n");
                    abort();
                    return r;
                }
            } else {
                membuf_reset(mptr);
            }
        }
    } else {
        if( (mptr = frame_get_channel_int(f,0)) == NULL) {
            membuf_init(&m);
            if( (r = membuf_append(&f->samples,&m,sizeof(membuf))) != 0) {
                fprintf(stderr,"out of memory\n");
                abort();
                return r;
            }
        } else {
            membuf_reset(mptr);
        }
    }

    return 0;
}

int frame_buffer(frame* f) {
    int r;
    size_t i;
    membuf* m;
    size_t samplesize;

    if(f->duration == 0) return -1;
    if( (r = frame_ready(f)) != 0) return r;

    samplesize = samplefmt_size(f->format);

    if(samplefmt_is_planar(f->format)) {
        for(i=0;i<f->channels;i++) {
            m = frame_get_channel_int(f,i);
            if( (r = membuf_ready(m, samplesize * f->duration)) != 0) {
                fprintf(stderr,"out of memory\n");
                abort();
                return r;
            }
        }
    } else {
        m = frame_get_channel_int(f,0);
        if( (r = membuf_ready(m, f->channels * samplesize * f->duration)) != 0) {
            fprintf(stderr,"out of memory\n");
            abort();
            return r;
        }
    }

    return 0;
}

int frame_fill(frame* f, unsigned int duration) {
    int r;
    size_t i;
    unsigned int old_duration;
    size_t samplesize;
    size_t llen;
    size_t rlen;
    membuf* m;

    if(duration <= f->duration) return 0;

    old_duration = f->duration;
    samplesize = samplefmt_size(f->format);
    llen = (size_t)old_duration * samplesize;
    rlen = ((size_t)duration - (size_t)old_duration) * samplesize;

    f->duration = duration;
    if( (r = frame_buffer(f)) != 0) return r;

    if(samplefmt_is_planar(f->format)) {
        for(i=0;i<f->channels;i++) {
            m = frame_get_channel_int(f,i);
            memset(&m->x[llen],0,rlen);
        }
    } else {
        m = frame_get_channel_int(f,0);
        llen *= (size_t)f->channels;
        rlen *= (size_t)f->channels;
        memset(&m->x[llen],0,rlen);
    }
    return 0;
}

int frame_copy(frame* dest, const frame* src) {
    int r;
    size_t i;
    size_t channels;
    size_t llen;
    size_t samplesize;
    const membuf* src_buf;
    membuf* dest_buf;

    dest->format   = src->format;
    dest->channels = src->channels;
    dest->duration = src->duration;
    dest->sample_rate = src->sample_rate;
    dest->pts = src->pts;

    if( (r = frame_buffer(dest)) != 0) return r;

    samplesize = samplefmt_size(dest->format);

    if(samplefmt_is_planar(dest->format)) {
        channels = dest->channels;
        llen = dest->duration;
    } else {
        channels = 1;
        llen = dest->duration * dest->channels;
    }
    llen *= samplesize;


    for(i=0;i<channels;i++) {
        src_buf = frame_get_channel_int(src,i);
        dest_buf = frame_get_channel_int(dest,i);
        memcpy(dest_buf->x,src_buf->x,llen);
    }

    return 0;
}

int frame_append_convert(frame* dest, const frame* src, samplefmt format) {
    int r;
    size_t i;
    const membuf* src_buf;
    membuf* dest_buf;
    int src_planar, dest_planar;
    size_t duration;

    if(dest->sample_rate != src->sample_rate) return -1;

    dest->format   = format;
    dest->channels = src->channels;
    duration       = dest->duration;
    dest->duration += src->duration;

    if( (r = frame_buffer(dest)) != 0) return r;

    src_planar = samplefmt_is_planar(src->format);
    dest_planar = samplefmt_is_planar(dest->format);

    if(src_planar && dest_planar) {
        for(i=0;i<src->channels;i++) {
            src_buf  = frame_get_channel_int(src,i);
            dest_buf = frame_get_channel_int(dest,i);
            samplefmt_convert(&dest_buf->x[duration * samplefmt_size(dest->format)],src_buf->x,src->format,dest->format, src->duration, 1, 0, 1, 0);
        }
        return 0;
    }

    if(!src_planar && !dest_planar) {
        src_buf  = frame_get_channel_int(src,0);
        dest_buf = frame_get_channel_int(dest,0);
        samplefmt_convert(&dest_buf->x[duration * samplefmt_size(dest->format) * dest->channels],src_buf->x,src->format,dest->format, src->duration * src->channels, 1, 0, 1, 0);
        return 0;
    }

    if(!src_planar && dest_planar) {
        for(i=0;i<src->channels;i++) {
            src_buf  = frame_get_channel_int(src,0);
            dest_buf = frame_get_channel_int(dest,i);
            samplefmt_convert(&dest_buf->x[duration * samplefmt_size(dest->format)],src_buf->x,src->format,dest->format, src->duration, src->channels, i, 1, 0);
        }
        return 0;
    }

    /* if(src_planar && !dest_planar) { */
        for(i=0;i<src->channels;i++) {
            src_buf  = frame_get_channel_int(src,i);
            dest_buf = frame_get_channel_int(dest,0);
            samplefmt_convert(&dest_buf->x[duration * samplefmt_size(dest->format) * dest->channels],src_buf->x,src->format,dest->format, src->duration, 1, 0, dest->channels, i);
        }

    /* } */

    return 0;
}

int frame_convert(frame* dest, const frame* src, samplefmt format) {
    dest->duration = 0;
    return frame_append_convert(dest,src,format);
}

int frame_append(frame* dest, const frame* src) {
    return frame_append_convert(dest,src,dest->format == SAMPLEFMT_UNKNOWN ? src->format : dest->format);
}

int frame_move(frame* dest, frame* src, unsigned int len) {
    int r;
    size_t i;
    size_t channels;
    size_t samplesize;
    size_t llen;
    size_t rlen;
    membuf *dest_buf;
    membuf *src_buf;

    if(src->duration < len) return -1;
    if(dest->channels != src->channels) return -1;
    if(dest->format != src->format) return -1;
    if(dest->sample_rate != src->sample_rate) return -1;

    dest->duration = len;
    dest->channels = src->channels;
    dest->format   = src->format;
    dest->pts      = src->pts;

    if( (r = frame_buffer(dest)) != 0) return r;

    if(samplefmt_is_planar(dest->format)) {
        channels = dest->channels;
        llen = len;
        rlen = src->duration - len;
    } else {
        channels = 1;
        llen = len * dest->channels;
        rlen = (src->duration - len) * dest->channels;
    }
    samplesize = samplefmt_size(dest->format);

    for(i=0;i<channels;i++) {
        dest_buf = frame_get_channel_int(dest,i);
        src_buf  = frame_get_channel_int(src,i);
        memmove(&dest_buf->x[0],&src_buf->x[0],samplesize*llen);
        if(rlen > 0) memmove(&src_buf->x[0],&src_buf->x[samplesize*llen],rlen*samplesize);
    }

    src->duration -= len;
    src->pts += len;
    return 0;
}

int frame_trim(frame* f, unsigned int len) {
    membuf *buf;
    size_t i;
    size_t samplesize;
    size_t llen;
    size_t rlen;

    samplesize = samplefmt_size(f->format);
    llen = (size_t)len * samplesize;
    rlen = ((size_t)f->duration - (size_t) len) * samplesize;

    if(samplefmt_is_planar(f->format)) {
        for(i=0;i<f->channels;i++) {
            buf = frame_get_channel_int(f,i);
            memmove(&buf->x[0],&buf->x[llen],rlen);
        }
    } else {
        llen *= (size_t)f->channels;
        rlen *= (size_t)f->channels;
        buf = frame_get_channel_int(f,0);
        memmove(&buf->x[0],&buf->x[llen],rlen);
    }

    f->duration -= len;
    f->pts += len;
    return 0;
}


const frame_receiver frame_receiver_zero = FRAME_RECEIVER_ZERO;
const frame_source frame_source_zero = FRAME_SOURCE_ZERO;
const frame frame_zero = FRAME_ZERO;

int frame_receiver_open_null(void* handle, const frame_source* source) {
    (void)handle;
    (void)source;
    fprintf(stderr,"[app error] frame_receiver open not set\n");
    abort();
    return -1;
}

int frame_receiver_submit_frame_null(void* handle, const frame* frame) {
    (void)handle;
    (void)frame;
    fprintf(stderr,"[app error] frame_receiver subit_frame not set\n");
    abort();
    return -1;
}

int frame_receiver_flush_null(void* handle) {
    (void)handle;
    fprintf(stderr,"[app error] frame_receiver flush not set\n");
    abort();
    return -1;
}

