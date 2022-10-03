#include "tag.h"

#include <string.h>
#include <stdlib.h>

void taglist_dump(const taglist* list, FILE* out) {
    size_t i;
    tag *t;
    for(i=0;i<taglist_len(list);i++) {
        t = taglist_get_tag(list,i);
        fprintf(out,"%.*s = %.*s, priority=%u, order=%u\n",
          (int)t->key.len,   (char *)t->key.x,
          (int)t->value.len, (char *)t->value.x,
          t->priority,(unsigned int)t->order);
    }
}

void tag_init(tag* t) {
    strbuf_init(&t->key);
    strbuf_init(&t->value);
    t->priority = 0xFF;
    t->order = 0;
}

void tag_free(tag* t) {
    strbuf_free(&t->key);
    strbuf_free(&t->value);
}

int tag_clone(tag* t, const tag* src) {
    int r;
    if( ( r = tag_set_key(t,&src->key) != 0)) return r;
    if( ( r = tag_set_value(t,&src->value) != 0)) return r;
    t->order = src->order;
    t->priority = src->priority;
    return 0;
}

int tag_set_key(tag* t, const strbuf* s) {
    return strbuf_copy(&t->key,s);
}

int tag_set_value(tag* t, const strbuf* s) {
    return strbuf_copy(&t->value,s);
}

void taglist_init(taglist* list) {
    membuf_init(&list->tags);
    list->sorted = 0;
}

void taglist_free(taglist* list) {
    taglist_reset(list);
    membuf_free(&list->tags);
}

void taglist_shallow_free(taglist* list) {
    membuf_free(&list->tags);
}

void taglist_reset(taglist* list) {
    size_t i = 0;
    size_t len = taglist_len(list);
    tag* tag;

    for(i=0;i<len;i++) {
        tag = taglist_get_tag(list,i);
        tag_free(tag);
    }
    membuf_reset(&list->tags);
}

size_t taglist_len(const taglist* list) {
    return list->tags.len / sizeof(tag);
}

size_t taglist_find(const taglist* list, const strbuf* key, size_t start) {
    tag* t;
    size_t i;
    size_t len;
    int r;

    len = list->tags.len / sizeof(tag);
    t = (tag*)list->tags.x;

    for(i = start; i < len; i++) {
        r = strbuf_casecmp(&t[i].key,key);
        if(r == 0) return i;
        if(list->sorted && r > 0) break;
    }

    return len;
}

size_t taglist_find_cstr(const taglist* list, const char* key, size_t start) {
    strbuf t;
    t.x = (uint8_t*)key;
    t.len = strlen(key);
    return taglist_find(list,&t,start);
}

tag* taglist_get_tag(const taglist* list, size_t index) {
    index *= sizeof(tag);
    if(index < list->tags.len) return (tag*)&list->tags.x[index];
    return NULL;
}

int taglist_add_tag(taglist* list, const tag* t) {
    list->sorted = 0;
    return membuf_append(&list->tags,t,sizeof(tag));
}

int taglist_add_priority_order(taglist* list, const strbuf* key, const strbuf* value, uint8_t priority, size_t order) {
    int r;
    tag t;

    tag_init(&t);
    if( (r = tag_set_key(&t,key)) != 0 ) {
        tag_free(&t);
        return r;
    }
    if( (r = tag_set_value(&t,value)) != 0 ) {
        tag_free(&t);
        return r;
    }
    t.priority = priority;
    t.order = order;

    if( (r = taglist_add_tag(list,&t)) != 0) {
        tag_free(&t);
        return r;
    }
    return 0;
}

int taglist_add_priority(taglist* list, const strbuf* key, const strbuf* value, uint8_t priority) {
    return taglist_add_priority_order(list,key,value,priority,taglist_len(list));
}

int taglist_add(taglist* list, const strbuf* key, const strbuf* value) {
    return taglist_add_priority(list,key,value,255);
}

int taglist_remove_tag(taglist* list, size_t index) {
    return membuf_remove(&list->tags, sizeof(tag), sizeof(tag)*index);
}

int taglist_del_tag(taglist* list, size_t index) {
    tag *t = taglist_get_tag(list,index);
    if(t == NULL) return -1;
    tag_free(t);
    return taglist_remove_tag(list,index);
}

int taglist_clear(taglist* list, const strbuf* key) {
    int r;
    size_t i = 0;
    while( (i = taglist_find(list,key,i)) != taglist_len(list)) {
        if( (r = taglist_del_tag(list,i)) != 0) return r;
    }
    return 0;
}

int taglist_clear_str(taglist* list, const char* key) {
    strbuf t;
    t.x = (uint8_t*)key;
    t.len = strlen(key);
    return taglist_clear(list,&t);
}

int taglist_deep_copy(taglist* dest, const taglist* src) {
    int r;
    tag tmp;
    tag* tag;
    size_t i;
    size_t len;

    taglist_reset(dest);

    len = taglist_len(src);
    for(i=0;i<len;i++) {
        tag_init(&tmp);

        tag = taglist_get_tag(src,i);
        if(tag == NULL) return -1;

        if( (r = tag_clone(&tmp,tag)) != 0) return r;
        if( (r = taglist_add_tag(dest,&tmp)) != 0) return r;
    }
    dest->sorted = src->sorted;

    return 0;
}

int taglist_shallow_copy(taglist* dest, const taglist* src) {
    int r;

    taglist_reset(dest);

    if( (r = membuf_copy(&dest->tags,&src->tags)) != 0) return r;
    dest->sorted = src->sorted;

    return 0;
}

static int tag_compare_order(const void* p1, const void* p2) {
    int r;
    const tag* t1 = (const tag*) p1;
    const tag* t2 = (const tag*) p2;

    if(t1->order == t2->order) {

        if( (r = strbuf_cmp(&t1->key,&t2->key)) != 0) return r;

        return t1->priority < t2->priority ? -1 : t1->priority > t2->priority ? 1 : 0;
    }

    return t1->order < t2->order ? -1 : 1;
}

static int tag_compare(const void* p1, const void* p2) {
    int r;
    const tag* t1 = (const tag*) p1;
    const tag* t2 = (const tag*) p2;

    if( (r = strbuf_cmp(&t1->key,&t2->key)) != 0) return r;

    if(t1->priority == t2->priority) {
        return t1->order < t2->order ? -1 : t1->order > t2->order ? 1 : 0;
    }

    return t1->priority < t2->priority ? -1 : 1;
}


void taglist_sort(taglist* list) {
    qsort(list->tags.x, taglist_len(list), sizeof(tag), tag_compare);
    list->sorted = 1;
}

void taglist_sort_order(taglist* list) {
    qsort(list->tags.x, taglist_len(list), sizeof(tag), tag_compare_order);
    list->sorted = 0;
}

int taglist_add_cstr_priority_order(taglist* list, const char* key, const char* value, uint8_t priority, size_t order) {
    strbuf k = STRBUF_ZERO;
    strbuf v = STRBUF_ZERO;

    k.x = (uint8_t*)key;
    v.x = (uint8_t*)value;

    k.len = strlen(key);
    v.len = strlen(value);

    return taglist_add_priority_order(list,&k,&v,priority,order);
}

int taglist_add_cstr_priority(taglist* list, const char* key, const char* value, uint8_t priority) {
    return taglist_add_cstr_priority_order(list,key,value,priority,taglist_len(list));
}

int taglist_add_cstr(taglist* list, const char* key, const char* value) {
    return taglist_add_cstr_priority(list,key,value,255);
}

int taglist_map(const taglist* map, const taglist* src, const taglist_map_flags* flags, taglist* out) {
    int r;
    size_t i;
    size_t idx;
    size_t found;
    tag* t = NULL;
    tag* m = NULL;
    strbuf tmp;
    taglist tmplist;

    taglist_init(&tmplist);
    taglist_reset(out);

    for(i=0;i<taglist_len(src);i++) {
        t = taglist_get_tag(src,i);
        found = 0;
        idx = 0;

        while( (idx = taglist_find(map,&t->key,idx)) != taglist_len(map)) {
            found = 1;
            m = taglist_get_tag(map,idx);

            if( (r = taglist_add_priority_order(&tmplist,&m->value,&t->value,m->priority,t->order)) != 0) return r;
            idx++;
        }

        if(!found) {
            if(flags->unknownmode == TAGMAP_UNKNOWN_IGNORE) continue;
            strbuf_init(&tmp);
            if( (r = strbuf_copy(&tmp,&t->key)) != 0) return r;
            strbuf_lower(&tmp);
            if( (r = membuf_prepend(&tmp,"TXXX:",5)) != 0) return r;
            if( (r = taglist_add_priority_order(&tmplist,&tmp,&t->value,0xFF,t->order)) != 0) return r;
            strbuf_free(&tmp);
        }
    }

    /* sort the temp taglist by key, priority, order */
    taglist_sort(&tmplist);

    /* now we go through the temp list and handle duplicate tags */
    i=0;
    while(i < taglist_len(&tmplist)) {
        t = taglist_get_tag(&tmplist,i);

        /* move the tag to out */
        if( (r = taglist_add_tag(out,t)) != 0) return r;
        taglist_remove_tag(&tmplist,i);
        /* update t to the new reference */
        t = taglist_get_tag(out,taglist_len(out)-1);

        if(i == taglist_len(&tmplist)) break; /* last tag */

        /* get the next tag that moved into the old slot */
        m = taglist_get_tag(&tmplist,i);

        while(strbuf_equals(&t->key,&m->key)) {
            if(t->priority == m->priority) {
                switch(flags->mergemode) {
                    case TAGMAP_MERGE_NULL: {
                        if( (r = strbuf_term(&t->value)) != 0) return r;
                        if( (r = strbuf_cat(&t->value,&m->value)) != 0) return r;
                        break;
                    }
                    case TAGMAP_MERGE_SEMICOLON: {
                        if( (r = membuf_append(&t->value,"; ",2)) != 0) return r;
                        if( (r = strbuf_cat(&t->value,&m->value)) != 0) return r;
                        break;
                    }
                    case TAGMAP_MERGE_IGNORE: /* fall-through */
                    default: break;
                }
            }
            i++;
            if(i == taglist_len(&tmplist)) break; /* last tag */
            m = taglist_get_tag(&tmplist,i);
        }
    }

    taglist_free(&tmplist);
    taglist_sort_order(out);

    return 0;
}
