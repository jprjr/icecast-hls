#include "membuf.h"

#include <string.h>
#include <stdlib.h>

#define BLOCKSIZE 512

void membuf_init(membuf* m) {
    m->x = NULL;
    m->a = 0;
    m->len = 0;
    m->blocksize = BLOCKSIZE;
}

void membuf_init_bs(membuf* m, size_t bs) {
    m->x = NULL;
    m->a = 0;
    m->len = 0;
    m->blocksize = bs;
}

void membuf_reset(membuf* m) {
    m->len = 0;
}

void membuf_free(membuf* m) {
    if(m->a != 0) free(m->x);
    m->x = NULL;
    m->len = 0;
    m->a = 0;
}

int membuf_ready(membuf* m, size_t len) {
    uint8_t *t;
    size_t a;
    if(len > m->a) {
        a = (len + (m->blocksize-1)) & -m->blocksize;
        t = realloc(m->x, a);
        if(t == NULL) return -1;
        m->x = t;
        m->a = a;
    }
    return 0;
}

int membuf_readyplus(membuf* m, size_t len) {
    return membuf_ready(m, m->len + len);
}

int membuf_insert(membuf* m, const void* src, size_t len, size_t pos) {
    int r;
    size_t ex = len;

    if(pos > m->len) ex += pos - m->len;

    if( (r = membuf_readyplus(m,ex)) != 0) return r;
    if(m->len > pos) {
        memmove(&m->x[pos+len],&m->x[pos],m->len - pos);
    }
    memcpy(&m->x[pos],src,len);
    m->len += ex;
    return 0;
}

int membuf_append(membuf* m, const void* src, size_t len) {
    int r;

    if(len == 0) return 0;
    if( (r = membuf_readyplus(m,len)) != 0) return r;
    memcpy(&m->x[m->len],src,len);
    m->len += len;
    return 0;
}

int membuf_prepend(membuf* m, const void* src, size_t len) {
    int r;
    if( (r = membuf_readyplus(m,len)) != 0) return r;
    if(m->len > 0) {
        memmove(&m->x[len],&m->x[0],m->len);
    }
    memcpy(&m->x[0],src,len);
    m->len += len;
    return 0;
}

int membuf_remove(membuf* m, size_t len, size_t pos) {
    if(pos + len > m->len) return -1;

    if(m->len > pos + len) {
        memmove(&m->x[pos],&m->x[pos+len],m->len - (pos+len));
    }
    m->len -= len;
    return 0;
}

int membuf_copy(membuf* m, const membuf* s) {
    m->len = 0;
    return membuf_append(m,s->x,s->len);
}

int membuf_cat(membuf* m, const membuf* s) {
    return membuf_append(m,s->x,s->len);
}

int membuf_discard(membuf *m, size_t len) {
    if(len > m->len) return -1;
    m->len -= len;
    return 0;
}

int membuf_trim(membuf *m, size_t len) {
    if(len > m->len) return -1;
    if(len < m->len) {
        memmove(&m->x[0],&m->x[len],m->len - len);
    }
    m->len -= len;
    return 0;
}
