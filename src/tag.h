#ifndef TAG_H
#define TAG_H

#include "strbuf.h"
#include "membuf.h"
#include <stdio.h>

struct tag {
    strbuf key;
    strbuf value;
    size_t order; /* used to restore the origina order, if needed */
    uint8_t priority; /* used in the tag-mapping process */
};
typedef struct tag tag;

struct taglist {
    membuf tags;
    uint8_t sorted;
};

typedef struct taglist taglist;

/* a callback function to be used by HTTP (Shoutcast) as
 * well as decoders (miniflac). It should return 0 on
 * success, anything else is a failure */

typedef int (*tag_handler_callback)(void* userdata, const taglist*);

struct tag_handler {
    tag_handler_callback cb;
    void* userdata;
};

typedef struct tag_handler tag_handler;

/* used for the taglist_map function */
enum tagmap_mergemode {
    TAGMAP_MERGE_IGNORE = 0,
    TAGMAP_MERGE_NULL = 1,
    TAGMAP_MERGE_SEMICOLON = 2,
};

typedef enum tagmap_mergemode tagmap_mergemode;

enum tagmap_unknownmode {
    TAGMAP_UNKNOWN_IGNORE = 0,
    TAGMAP_UNKNOWN_TXXX = 1,
};

typedef enum tagmap_unknownmode tagmap_unknownmode;

struct taglist_map_flags {
    tagmap_mergemode mergemode;
    tagmap_unknownmode unknownmode;
    int passthrough;
};

typedef struct taglist_map_flags taglist_map_flags;

#ifdef __cplusplus
extern "C" {
#endif

void tag_init(tag*);
void tag_free(tag*);

int tag_clone(tag*, const tag*);

int tag_set_key(tag*, const strbuf* key);
int tag_set_value(tag*, const strbuf* val);

void taglist_init(taglist*);
void taglist_free(taglist*);
void taglist_reset(taglist*); /* frees the tags but keeps the tag pointer storage */

size_t taglist_len(const taglist*);

/* makes a copy of a taglist, including all actual tag data */
int taglist_deep_copy(taglist* dest, const taglist* src);

/* makes a shallow copy - only copies tag references, does not
 * copy actual tag data */
int taglist_shallow_copy(taglist* dest, const taglist* src);
/* does a shallow free - only frees the list of references,
 * actual tags are still in memory */
void taglist_shallow_free(taglist* dest);

/* returns the index of a tag, starting at index,
 * returns the length of the list if the tag isn't found */
size_t taglist_find(const taglist*, const strbuf* key, size_t start);

/* version that uses a regular c string */
size_t taglist_find_cstr(const taglist*, const char* key, size_t start);


/* get a tag by index */
tag* taglist_get_tag(const taglist*, size_t index);

/* adds a tag reference to the list, memory is now
 * managed by the list - as in, calling taglist_free
 * or taglist_reset will delete the tag */
int taglist_add_tag(taglist*, const tag* tag);

/* add a tag with just key/value, makes a copy of key and value,
 * equivalent to calling tag_set_key, tag_set_value, taglist_add_tag all
 * in one go. */
int taglist_add(taglist*, const strbuf* key, const strbuf* value);
int taglist_add_cstr(taglist*, const char* key, const char* value);

int taglist_add_priority_order(taglist*, const strbuf* key, const strbuf* value, uint8_t priority, size_t order);
int taglist_add_priority(taglist*, const strbuf* key, const strbuf* value, uint8_t priority);
int taglist_add_cstr_priority_order(taglist*, const char* key, const char* value, uint8_t priority, size_t order);
int taglist_add_cstr_priority(taglist*, const char* key, const char* value, uint8_t priority);

/* removes a tag reference, used when moving a tag from one list to another */
int taglist_remove_tag(taglist*, size_t index);
int taglist_del_tag(taglist*, size_t index);

/* clears any tags with the given key */
int taglist_clear(taglist*, const strbuf* key);

/* clears any tags with the given key - C String version */
int taglist_clear_str(taglist*, const char* key);

/* sorts tags by key, then priority, then their original order */
void taglist_sort(taglist*);

/* sorts tags by their original order, then key, then priority */
void taglist_sort_order(taglist*);

void taglist_dump(const taglist* list, FILE* out);

/* uses map to re-map tags into output. your map HAS to have been
 * sorted with taglist_sort first!! */
int taglist_map(const taglist *map, const taglist* tags, const taglist_map_flags* flags, taglist* out);

#ifdef __cplusplus
}
#endif

#endif
