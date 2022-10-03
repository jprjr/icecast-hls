#include "tagmap.h"
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

static int find_whitespace(strbuf *p, const strbuf* src) {
    p->len = src->len;
    p->x = src->x;

    while(p->len) {
        switch(tolower(p->x[0])) {
            case ' ': /* fall-through */
            case '\t': return 0;
            default: break;
        }
        p->len--;
        p->x = &p->x[1];
    }
    return -1;
}

static int skip_whitespace(strbuf *p) {
    while(p->len) {
        switch(tolower(p->x[0])) {
            case ' ': /* fall-through */
            case '\t': break;
            default: return 0;
        }
        p->len--;
        p->x = &p->x[1];
    }
    return -1;
}

void tagmap_entry_init(tagmap_entry* e) {
    strbuf_init(&e->id);
    taglist_init(&e->map);
}

void tagmap_entry_free(tagmap_entry* e) {
    strbuf_free(&e->id);
    taglist_free(&e->map);
}

void tagmap_init(tagmap* m) {
    membuf_init(m);
}

void tagmap_free(tagmap* m) {
    size_t i = 0;
    tagmap_entry *e = NULL;
    for(i=0;i<tagmap_len(m);i++) {
        e = tagmap_get(m,i);
        tagmap_entry_free(e);
    }
    membuf_free(m);
}

size_t tagmap_len(const tagmap* m) {
    return m->len / sizeof(tagmap_entry);
}

tagmap_entry* tagmap_find(const tagmap* m, const strbuf* id) {
    size_t i = 0;
    tagmap_entry *e = NULL;
    for(i=0;i<tagmap_len(m);i++) {
        e = tagmap_get(m,i);
        if(strbuf_equals(id,&e->id)) return e;
    }
    return NULL;
}

tagmap_entry* tagmap_get(const tagmap* m, size_t index) {
    index *= sizeof(tagmap_entry);
    return (tagmap_entry*) &m->x[index];
}

static int tagmap_entry_config(taglist* map, const strbuf* key, const strbuf* value) {
    int r;
    strbuf k;
    strbuf v;
    strbuf p;
    uint8_t priority;
    size_t len;

    strbuf_init(&k); strbuf_init(&v); strbuf_init(&p);
    priority = 0;
    len = 0;

    len = value->len;

    /* parse val for priority= values */
    if( find_whitespace(&p,value) == 0) {
        len -= p.len;

        if(skip_whitespace(&p) != 0) goto add;
        if(!strbuf_begins_cstr(&p,"priority=")) goto add;

        p.x = &p.x[strlen("priority=")];
        p.len -= strlen("priority=");
        if(p.len == 0) goto add;
        errno = 0;
        priority = strbuf_strtoul(&p,10);
        if(errno == ENOMEM) return -1;
    }

    add:

    if(len < 4) return -1;

    if( (r = strbuf_copy(&k,key)) != 0) return -1;
    strbuf_lower(&k);

    if( (r = membuf_append(&v,value->x,4)) != 0) {
        strbuf_free(&k);
        return -1;
    }
    strbuf_upper(&v);
    len -= 4;
    if(len) {
        if( (r = membuf_append(&v,&value->x[4],len)) != 0) {
            strbuf_free(&k);
            strbuf_free(&v);
            return -1;
        }
    }

    r = taglist_add_priority(map,&k,&v,priority);
    strbuf_free(&k);
    strbuf_free(&v);
    return r;
}

int tagmap_configure(const strbuf* id, const strbuf* key, const strbuf* value, tagmap* m) {
    int r;
    tagmap_entry *e;
    tagmap_entry te;

    e = tagmap_find(m,id);
    if( e == NULL) {
        tagmap_entry_init(&te);
        if( (r = strbuf_copy(&te.id,id)) != 0) return r;
        if( (r = membuf_append(m,&te,sizeof(tagmap_entry))) != 0) return r;
        e = tagmap_find(m,id);
        if(e == NULL) abort();
    }

    return tagmap_entry_config(&e->map,key,value);
}
