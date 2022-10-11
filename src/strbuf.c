#include "strbuf.h"

#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#endif

#define BLOCKSIZE 16

void strbuf_init(strbuf* s) {
    return membuf_init_bs(s,BLOCKSIZE);
}

void strbuf_init_bs(strbuf* s,size_t bs) {
    return membuf_init_bs(s,bs);
}

void strbuf_free(strbuf* s) {
    membuf_free(s);
}

void strbuf_reset(strbuf* s) {
    membuf_reset(s);
}

int strbuf_ready(strbuf* s, size_t a) {
    return membuf_ready(s,a);
}

int strbuf_readyplus(strbuf* s, size_t a) {
    return membuf_readyplus(s,a);
}

char* strbuf_chr(const strbuf* s, char c) {
    return memchr(s->x,c,s->len);
}

char* strbuf_rchr(const strbuf* s, char c) {
    char* t = (char *)s->x;
    size_t n = s->len;
    while(n--) {
        if(t[n] == c) return &t[n];
    }
    return NULL;
}


int strbuf_cmp(const strbuf* s1, const strbuf* s2) {
    size_t min = s1->len < s2->len ? s1->len : s2->len;
    int r = memcmp(s1->x,s2->x,min);

    if(r == 0) {
        if(s1->len < s2->len) return 0 - s2->x[s1->len];
        if(s1->len > s2->len) return s1->x[s2->len];
    }

    return r;
}

int strbuf_copy(strbuf* d, const strbuf* s) {
    return membuf_copy(d,s);
}

int strbuf_cat(strbuf* d, const strbuf* s) {
    return membuf_cat(d,s);
}

int strbuf_append(strbuf* d, const char* src, size_t len) {
    return membuf_append(d,src,len);
}

int strbuf_append_cstr(strbuf* d, const char* src) {
    return membuf_append(d,src,strlen(src));
}

int strbuf_equals(const strbuf* s1, const strbuf* s2) {
    return strbuf_cmp(s1,s2) == 0;
}

int strbuf_begins(const strbuf* s, const strbuf* q) {
    strbuf t = STRBUF_ZERO;
    if(s->len < q->len) return 0;
    t.x = s->x;
    t.len = q->len;
    return strbuf_cmp(&t,q) == 0;
}

int strbuf_ends(const strbuf* s, const strbuf *q) {
    strbuf t = STRBUF_ZERO;
    if(s->len < q->len) return 0;
    t.len = q->len;
    t.x = &s->x[s->len - q->len];
    return strbuf_cmp(&t,q) == 0;
}

int strbuf_contains(const strbuf* s1, const strbuf* s2) {
    strbuf t = STRBUF_ZERO;
    t.len = s1->len;
    t.x = s1->x;
    while(t.len >= s2->len) {
        if(strbuf_begins(&t,s2)) return 1;
        t.len--;
        t.x++;
    }
    return 0;
}

int strbuf_casecontains(const strbuf* s1, const strbuf* s2) {
    strbuf t = STRBUF_ZERO;
    t.len = s1->len;
    t.x = s1->x;
    while(t.len >= s2->len) {
        if(strbuf_casebegins(&t,s2)) return 1;
        t.len--;
        t.x++;
    }
    return 0;
}

int strbuf_contains_cstr(const strbuf* s1, const char* s) {
    strbuf s2 = STRBUF_ZERO;
    s2.x = (uint8_t*)s;
    s2.len = strlen(s);
    return strbuf_contains(s1,&s2);
}

int strbuf_casecontains_cstr(const strbuf* s1, const char* s) {
    strbuf s2 = STRBUF_ZERO;
    s2.x = (uint8_t*)s;
    s2.len = strlen(s);
    return strbuf_casecontains(s1,&s2);
}

int strbuf_casecmp(const strbuf* s1, const strbuf* s2) {
    size_t i = 0;
    uint8_t c1 = 0;
    uint8_t c2 = 0;

    while(i < s1->len && i < s2->len && tolower(s1->x[i]) == tolower(s2->x[i])) {
        i++;
    }
    c1 = i >= s1->len ? 0 : tolower(s1->x[i]);
    c2 = i >= s2->len ? 0 : tolower(s2->x[i]);
    return c1 - c2;
}

int strbuf_caseequals(const strbuf* s1, const strbuf* s2) {
    return strbuf_casecmp(s1,s2) == 0;
}

int strbuf_casebegins(const strbuf* s, const strbuf* q) {
    strbuf t = STRBUF_ZERO;
    if(s->len < q->len) return 0;
    t.x = s->x;
    t.len = q->len;
    return strbuf_casecmp(&t,q) == 0;
}

int strbuf_caseends(const strbuf* s, const strbuf *q) {
    strbuf t = STRBUF_ZERO;
    if(s->len < q->len) return 0;
    t.len = q->len;
    t.x = &s->x[s->len - q->len];
    return strbuf_casecmp(&t,q) == 0;
}

int strbuf_equal(const strbuf* s1, const char* s2, size_t len) {
    strbuf t = STRBUF_ZERO;
    t.len = len;
    t.x = (uint8_t*)s2;
    return strbuf_equals(s1,&t);
}

int strbuf_begin(const strbuf* s, const char* q, size_t len) {
    strbuf t = STRBUF_ZERO;
    t.len = len;
    t.x = (uint8_t*)q;

    return strbuf_begins(s,&t);
}

int strbuf_end(const strbuf* s, const char* q, size_t len) {
    strbuf t = STRBUF_ZERO;
    t.len = len;
    t.x = (uint8_t*)q;

    return strbuf_ends(s,&t);
}

int strbuf_chrbuf(strbuf *d, const strbuf* s, char c) {
    char* t;
    size_t pos;

    t = strbuf_chr(s,c);
    if(t == NULL) return -1;

    pos = (t - (char *)s->x);
    d->a = 0;
    d->x = (uint8_t *)t;
    d->len = s->len - pos;
    return 0;
}

int strbuf_rchrbuf(strbuf *d, const strbuf* s, char c) {
    char* t;
    size_t pos;

    t = strbuf_rchr(s,c);
    if(t == NULL) return -1;

    pos = (t - (char *)s->x);
    d->a = 0;
    d->x = (uint8_t *)t;
    d->len = s->len - pos;
    return 0;
}

int strbuf_term(strbuf* d) {
    int r;
    if( (r = membuf_readyplus(d,1)) != 0) return r;
    d->x[d->len++] = '\0';

    return 0;
}

int strbuf_unterm(strbuf* d) {
    while(d->len && d->x[d->len] == '\0') {
        d->len--;
    }
    return 0;
}

#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
int strbuf_wide(strbuf* d, const strbuf* s) {
    int wide_len;
    int r;

    wide_len = MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,(char *)s->x,s->len,NULL,0);
    if(wide_len == 0) return -1;

    if( (r = membuf_ready(d,sizeof(wchar_t)*wide_len)) != 0) return r;
    d->len = 0;
    if( MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,(char *)s->x,s->len,(wchar_t*)d->x,wide_len) != wide_len) return -1;
    d->len = sizeof(wchar_t)*wide_len;
    return 0;
}

int strbuf_wideterm(strbuf* d) {
    int r;
    if( (r = membuf_readyplus(d,sizeof(wchar_t))) != 0) return r;
    memset(&d->x[d->len],0,sizeof(wchar_t));
    d->len += sizeof(wchar_t);
    return 0;
}
#endif

long strbuf_strtol(const strbuf* s, int base) {
    long r;
    strbuf tmp = STRBUF_ZERO;

    errno = 0;
    if( (r = strbuf_copy(&tmp,s)) != 0) {
        errno = ENOMEM;
        return 0;
    }

    if( (r = strbuf_term(&tmp)) != 0) {
        errno = ENOMEM;
        return 0;
    }

    r = strtol((const char *)tmp.x, NULL, base);
    strbuf_free(&tmp);
    return r;
}

long long strbuf_strtoll(const strbuf* s, int base) {
    long long r;
    strbuf tmp = STRBUF_ZERO;

    errno = 0;
    if( (r = strbuf_copy(&tmp,s)) != 0) {
        errno = ENOMEM;
        return 0;
    }

    if( (r = strbuf_term(&tmp)) != 0) {
        errno = ENOMEM;
        return 0;
    }

    r = strtoll((const char *)tmp.x, NULL, base);
    strbuf_free(&tmp);
    return r;
}

unsigned long strbuf_strtoul(const strbuf* s, int base) {
    unsigned long r;
    strbuf tmp = STRBUF_ZERO;

    errno = 0;
    if( (r = strbuf_copy(&tmp,s)) != 0) {
        errno = ENOMEM;
        return 0;
    }

    if( (r = strbuf_term(&tmp)) != 0) {
        errno = ENOMEM;
        return 0;
    }

    r = strtoul((const char *)tmp.x, NULL, base);
    strbuf_free(&tmp);
    return r;
}

unsigned long long strbuf_strtoull(const strbuf* s, int base) {
    unsigned long long r;
    strbuf tmp = STRBUF_ZERO;

    errno = 0;
    if( (r = strbuf_copy(&tmp,s)) != 0) {
        errno = ENOMEM;
        return 0;
    }

    if( (r = strbuf_term(&tmp)) != 0) {
        errno = ENOMEM;
        return 0;
    }

    r = strtoull((const char *)tmp.x, NULL, base);
    strbuf_free(&tmp);
    return r;
}

double strbuf_strtod(const strbuf* s) {
    double r;
    strbuf tmp = STRBUF_ZERO;

    errno = 0;
    if( (r = strbuf_copy(&tmp,s)) != 0) {
        errno = ENOMEM;
        return 0;
    }

    if( (r = strbuf_term(&tmp)) != 0) {
        errno = ENOMEM;
        return 0;
    }

    r = strtod((const char *)tmp.x, NULL);
    strbuf_free(&tmp);
    return r;
}

float strbuf_strtof(const strbuf* s) {
    float r;
    strbuf tmp = STRBUF_ZERO;

    errno = 0;
    if( (r = strbuf_copy(&tmp,s)) != 0) {
        errno = ENOMEM;
        return 0;
    }

    if( (r = strbuf_term(&tmp)) != 0) {
        errno = ENOMEM;
        return 0;
    }

    r = strtod((const char *)tmp.x, NULL);
    strbuf_free(&tmp);
    return r;
}

void strbuf_lower(strbuf *s) {
    size_t i = 0;
    for(i=0;i<s->len;i++) {
        s->x[i] = tolower(s->x[i]);
    }
}

void strbuf_upper(strbuf *s) {
    size_t i = 0;
    for(i=0;i<s->len;i++) {
        s->x[i] = toupper(s->x[i]);
    }
}

int strbuf_equals_cstr(const strbuf* s1, const char* s2) {
    strbuf tmp = STRBUF_ZERO;
    tmp.x = (uint8_t*)s2;
    tmp.len = strlen(s2);
    return strbuf_equals(s1,&tmp);
}

int strbuf_begins_cstr(const strbuf* s1, const char* s2) {
    strbuf tmp = STRBUF_ZERO;
    tmp.x = (uint8_t*)s2;
    tmp.len = strlen(s2);
    return strbuf_begins(s1,&tmp);
}

int strbuf_ends_cstr(const strbuf* s1, const char* s2) {
    strbuf tmp = STRBUF_ZERO;
    tmp.x = (uint8_t*)s2;
    tmp.len = strlen(s2);
    return strbuf_ends(s1,&tmp);
}

int strbuf_casecmp_cstr(const strbuf* s1, const char* s2) {
    strbuf tmp = STRBUF_ZERO;
    tmp.x = (uint8_t*)s2;
    tmp.len = strlen(s2);
    return strbuf_casecmp(s1,&tmp);
}

int strbuf_caseequals_cstr(const strbuf* s1, const char* s2) {
    strbuf tmp = STRBUF_ZERO;
    tmp.x = (uint8_t*)s2;
    tmp.len = strlen(s2);
    return strbuf_caseequals(s1,&tmp);
}

int strbuf_casebegins_cstr(const strbuf* s1, const char* s2) {
    strbuf tmp = STRBUF_ZERO;
    tmp.x = (uint8_t*)s2;
    tmp.len = strlen(s2);
    return strbuf_casebegins(s1,&tmp);
}

int strbuf_caseends_cstr(const strbuf* s1, const char* s2) {
    strbuf tmp = STRBUF_ZERO;
    tmp.x = (uint8_t*)s2;
    tmp.len = strlen(s2);
    return strbuf_caseends(s1,&tmp);
}

int strbuf_truthy(const strbuf* s) {
    return strbuf_equals_cstr(s,"true")
           || strbuf_equals_cstr(s,"1")
           || strbuf_equals_cstr(s,"yes")
           || strbuf_equals_cstr(s,"on")
           || strbuf_equals_cstr(s,"enable");
}

int strbuf_falsey(const strbuf* s) {
    return strbuf_equals_cstr(s,"false")
           || strbuf_equals_cstr(s,"0")
           || strbuf_equals_cstr(s,"no")
           || strbuf_equals_cstr(s,"off")
           || strbuf_equals_cstr(s,"disable");
}

int strbuf_sprintf(strbuf* d, const char *fmt, ...) {
    va_list args_list;
    int needed;
    int r;
    size_t s;

    va_start(args_list, fmt);
    needed = vsnprintf(NULL,0,fmt,args_list);
    va_end(args_list);

    if(needed < 0) return -1;

    s = (size_t)needed+1;

    if( (r = strbuf_readyplus(d,s)) != 0) return r;
    va_start(args_list, fmt);
    needed = vsnprintf((char *)&d->x[d->len],s,fmt,args_list);
    va_end(args_list);
    d->len += s-1;

    return 0;
}
