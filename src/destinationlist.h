#ifndef DESTINATIONLIST_H
#define DESTINATIONLIST_H

#include "membuf.h"
#include "destination.h"
#include "destination_sync.h"
#include "thread.h"
#include "tag.h"

struct destinationlist_entry {
    strbuf id;
    thread_ptr_t thread;
    destination_sync sync;
    destination destination;
};

typedef struct destinationlist_entry destinationlist_entry;

typedef membuf destinationlist;

#ifdef __cplusplus
extern "C" {
#endif

void destinationlist_init(destinationlist*);
void destinationlist_free(destinationlist*);

void destinationlist_entry_init(destinationlist_entry*);
void destinationlist_entry_free(destinationlist_entry*);

destinationlist_entry* destinationlist_find(const destinationlist* list, const strbuf* id);
destinationlist_entry* destinationlist_get(const destinationlist* list, size_t index);

size_t destinationlist_length(const destinationlist* list);

int destinationlist_configure(const strbuf* id, const strbuf* key, const strbuf* value, destinationlist* list);

int destinationlist_open(const destinationlist* list, const ich_time* now);

/* spawns threads for each destination in the list */
int destinationlist_start(const destinationlist* list);

/* waits for all threads to complete */
int destinationlist_wait(const destinationlist* list);

#ifdef __cplusplus
}
#endif

#endif
