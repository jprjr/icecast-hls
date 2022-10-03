#ifndef MAP_H
#define MAP_H

#include "membuf.h"
#include "strbuf.h"

#include <stdint.h>
#include <stddef.h>

/* a struct for having a bunch of strbuf keys
 * with values. keys can be anything and
 * must be unique. */

struct map_entry {
    strbuf key;
    union {
        strbuf   str;
        uint64_t u64;
        uint32_t u32;
        uint16_t u16;
        uint8_t   u8;
        int64_t  i64;
        int32_t  i32;
        int16_t  i16;
        int8_t    i8;
        float      f;
        double     d;
        void*    ptr;
        size_t     s;
    } value;
    uint8_t strflag;
};

typedef struct map_entry map_entry;

struct map {
    membuf buckets[256]; /* each bucket will be indexed by first byte, then a linear search in the bucket */
};

typedef struct map map;

#ifdef __cplusplus
extern "C" {
#endif

void map_init(map*);
void map_free(map*);

map_entry* map_find(const map*, const strbuf* key);
map_entry* map_find_lc(const map*, const strbuf* key);
map_entry* map_find_uc(const map*, const strbuf* key);

/*searches both uppercase and lowercase buckets */
map_entry* map_find_ac(const map*, const strbuf* key);

int map_add_str(map*,const strbuf* key, const strbuf* value);

int map_add_u64( map*, const strbuf* key, uint64_t);
int map_add_u32( map*, const strbuf* key, uint32_t);
int map_add_u16( map*, const strbuf* key, uint16_t);
int map_add_u8(  map*, const strbuf* key,  uint8_t);
int map_add_i64( map*, const strbuf* key,  int64_t);
int map_add_i32( map*, const strbuf* key,  int32_t);
int map_add_i16( map*, const strbuf* key,  int16_t);
int map_add_i8(  map*, const strbuf* key,   int8_t);
int map_add_f(   map*, const strbuf* key,    float);
int map_add_d(   map*, const strbuf* key,   double);
int map_add_ptr( map*, const strbuf* key,    void*);
int map_add_s(   map*, const strbuf* key,   size_t);

int map_add_cstr_u64( map*, const char* key, uint64_t);
int map_add_cstr_u32( map*, const char* key, uint32_t);
int map_add_cstr_u16( map*, const char* key, uint16_t);
int map_add_cstr_u8(  map*, const char* key,  uint8_t);
int map_add_cstr_i64( map*, const char* key,  int64_t);
int map_add_cstr_i32( map*, const char* key,  int32_t);
int map_add_cstr_i16( map*, const char* key,  int16_t);
int map_add_cstr_i8(  map*, const char* key,   int8_t);
int map_add_cstr_f(   map*, const char* key,    float);
int map_add_cstr_d(   map*, const char* key,   double);
int map_add_cstr_ptr( map*, const char* key,    void*);
int map_add_cstr_s(   map*, const char* key,   size_t);

#ifdef __cplusplus
}
#endif

#endif
