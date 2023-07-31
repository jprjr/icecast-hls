#ifndef SOURCELIST_H
#define SOURCELIST_H

#include "membuf.h"
#include "source.h"
#include "thread.h"
#include "ich_time.h"

typedef void (*sourcelist_quit_func)(void*,int);

struct sourcelist_entry {
    strbuf id;
    int loglevel;
    thread_ptr_t thread;
    thread_atomic_int_t status;
    source source;
    membuf destination_syncs; /* stores pointers to destination_sync objects */
    sourcelist_quit_func quit; /* used to end all threads when one dies */
    void* quit_userdata;
    size_t samplecount; /* counts number of samples seen */
    ich_time ts; /* timestamp to track datarate */
};

typedef struct sourcelist_entry sourcelist_entry;

typedef struct membuf sourcelist;

#ifdef __cplusplus
extern "C" {
#endif

void sourcelist_init(sourcelist*);
void sourcelist_free(sourcelist*);

void sourcelist_entry_init(sourcelist_entry*);
void sourcelist_entry_free(sourcelist_entry*);
void sourcelist_entry_dump_counters(const sourcelist_entry*);

size_t sourcelist_length(const sourcelist* list);
sourcelist_entry* sourcelist_find(const sourcelist* list, const strbuf* id);
sourcelist_entry* sourcelist_get(const sourcelist* list, size_t index);


int sourcelist_configure(const strbuf* id, const strbuf* key, const strbuf* value, sourcelist* list);

int sourcelist_open(const sourcelist* list, uint8_t shortflag);

/* spawns threads for each source in the list */
int sourcelist_start(const sourcelist* list);

/* waits for all threads to complete */
int sourcelist_wait(const sourcelist* list);

void sourcelist_quit(const sourcelist* list,int status);

void sourcelist_dump_counters(const sourcelist* list);

#ifdef __cplusplus
}
#endif

#endif
