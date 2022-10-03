#include "map.h"
#include <string.h>
#include <ctype.h>

#define MAP_ENTRY_ZERO { .key = STRBUF_ZERO , .value.u64 = 0, .strflag = 0 }

void map_init(map* m) {
    size_t i;
    for(i=0;i<256;i++) {
        membuf_init(&m->buckets[i]);
    }
}

void map_free(map* m) {
    size_t i, j;
    map_entry *e;
    for(i=0;i<256;i++) {
        e = (map_entry*)m->buckets[i].x;
        for(j=0;j<m->buckets[i].len / sizeof(map_entry);j++) {
            strbuf_free(&e[j].key);
            if(e[j].strflag) strbuf_free(&e[j].value.str);
        }
        membuf_free(&m->buckets[i]);
    }
}

map_entry* map_find(const map* m, const strbuf* key) {
    size_t i, len;
    map_entry *e;

    e = (map_entry*)m->buckets[key->x[0]].x;
    len = m->buckets[key->x[0]].len / sizeof(map_entry);

    for(i=0;i<len;i++) {
        if(strbuf_equals(key,&e[i].key)) return &e[i];
    }
    return NULL;
}

map_entry* map_find_uc(const map* m, const strbuf* key) {
    size_t i, len;
    map_entry *e;

    e = (map_entry*)m->buckets[toupper(key->x[0])].x;
    len = m->buckets[toupper(key->x[0])].len / sizeof(map_entry);

    for(i=0;i<len;i++) {
        if(strbuf_caseequals(key,&e[i].key)) return &e[i];
    }

    return NULL;
}

map_entry* map_find_lc(const map* m, const strbuf* key) {
    size_t i, len;
    map_entry *e;

    e = (map_entry*)m->buckets[tolower(key->x[0])].x;
    len = m->buckets[tolower(key->x[0])].len / sizeof(map_entry);

    for(i=0;i<len;i++) {
        if(strbuf_caseequals(key,&e[i].key)) return &e[i];
    }

    return NULL;
}

map_entry* map_find_ac(const map* m, const strbuf* key) {
    map_entry *e;

    e = map_find_lc(m,key);
    if(e == NULL) {
        if(toupper(key->x[0]) != tolower(key->x[0])) return map_find_uc(m,key);
    }

    return e;
}

int map_add_str(map* m, const strbuf* key, const strbuf* val) {
    map_entry e = MAP_ENTRY_ZERO;

    if(map_find(m,key) != NULL) return -2;

    strbuf_init(&e.key);
    strbuf_init(&e.value.str);
    e.strflag = 1;

    if(strbuf_copy(&e.key,key) != 0) return -1;
    if(strbuf_copy(&e.value.str,val) != 0) {
        strbuf_free(&e.key);
        return -1;
    }

    if(membuf_append(&m->buckets[key->x[0]],&e,sizeof(map_entry)) != 0) {
        strbuf_free(&e.key);
        strbuf_free(&e.value.str);
        return -1;
    }

    return 0;
}

#define gen_add_func(type,name) \
int map_add_ ## name (map* m, const strbuf* key, type val) { \
    map_entry e = MAP_ENTRY_ZERO; \
    if(map_find(m,key) != NULL) return -2; \
    strbuf_init(&e.key); \
    if(strbuf_copy(&e.key,key) != 0) return -1; \
    e.value.name = val; \
    if(membuf_append(&m->buckets[key->x[0]],&e,sizeof(map_entry)) != 0) { \
        strbuf_free(&e.key); \
        return -1; \
    } \
    return 0; \
}

#define gen_add_cstr_func(type,name) \
int map_add_cstr_ ## name (map* m, const char* key, type val) { \
    strbuf tmp = { .a = 0, .x = (uint8_t*)key, .len = strlen(key) }; \
    return map_add_ ## name (m, &tmp, val); \
}

gen_add_func(uint64_t,u64)
gen_add_func(uint32_t,u32)
gen_add_func(uint16_t,u16)
gen_add_func(uint8_t,u8)

gen_add_func(int64_t,i64)
gen_add_func(int32_t,i32)
gen_add_func(int16_t,i16)
gen_add_func(int8_t,i8)

gen_add_func(float,f);
gen_add_func(double,d);
gen_add_func(void*,ptr);
gen_add_func(size_t,s);

gen_add_cstr_func(uint64_t,u64)
gen_add_cstr_func(uint32_t,u32)
gen_add_cstr_func(uint16_t,u16)
gen_add_cstr_func(uint8_t,u8)

gen_add_cstr_func(int64_t,i64)
gen_add_cstr_func(int32_t,i32)
gen_add_cstr_func(int16_t,i16)
gen_add_cstr_func(int8_t,i8)

gen_add_cstr_func(float,f);
gen_add_cstr_func(double,d);
gen_add_cstr_func(void*,ptr);
gen_add_cstr_func(size_t,s);
