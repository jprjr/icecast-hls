#ifndef TAGMAP_H
#define TAGMAP_H

#include "membuf.h"
#include "strbuf.h"
#include "tag.h"

/* basically just a taglist plus an ID field,
 * used to link up maps with destinations */

struct tagmap_entry {
    strbuf id;
    taglist map;
};

typedef struct tagmap_entry tagmap_entry;

typedef membuf tagmap;

#ifdef __cplusplus
extern "C" {
#endif

void tagmap_entry_init(tagmap_entry*);
void tagmap_entry_free(tagmap_entry*);

void tagmap_init(tagmap*);
void tagmap_free(tagmap*);

tagmap_entry* tagmap_find(const tagmap* maps, const strbuf* id);
tagmap_entry* tagmap_get(const tagmap* maps, size_t index);
size_t tagmap_len(const tagmap* maps);

int tagmap_configure(const strbuf* id, const strbuf* key, const strbuf* value, tagmap* map);

#ifdef __cplusplus
}
#endif

#endif
