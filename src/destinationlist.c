#include "destinationlist.h"
#include <stdlib.h>

size_t destinationlist_length(const destinationlist* dlist) {
    return dlist->len / sizeof(destinationlist_entry);
}

void destinationlist_init(destinationlist* dlist) {
    membuf_init(dlist);
}

void destinationlist_free(destinationlist* dlist) {
    size_t i;
    size_t len;

    destinationlist_entry* entry = (destinationlist_entry *)dlist->x;
    len = dlist->len / sizeof(destinationlist_entry);

    for(i=0;i<len;i++) {
        destinationlist_entry_free(&entry[i]);
    }

    membuf_free(dlist);
}

void destinationlist_entry_init(destinationlist_entry* entry) {
    strbuf_init(&entry->id);
    destination_sync_init(&entry->sync);
    destination_init(&entry->destination);
}

void destinationlist_entry_free(destinationlist_entry* entry) {
    strbuf_free(&entry->id);
    destination_sync_free(&entry->sync);
    destination_free(&entry->destination);
}

destinationlist_entry* destinationlist_find(const destinationlist* list, const strbuf* id) {
    size_t i;
    size_t len;

    destinationlist_entry* entry = (destinationlist_entry *)list->x;
    len = list->len / sizeof(destinationlist_entry);

    for(i=0;i<len;i++) {
        if(strbuf_equals(&entry[i].id,id)) return &entry[i];
    }
    return NULL;
}

destinationlist_entry* destinationlist_get(const destinationlist* list, size_t index) {
    destinationlist_entry* entry = (destinationlist_entry *)list->x;
    if(index < destinationlist_length(list)) {
        return &entry[index];
    }
    return NULL;
}

int destinationlist_configure(const strbuf* id, const strbuf* key, const strbuf* value, destinationlist* list) {
    int r;
    destinationlist_entry empty;
    destinationlist_entry* entry = destinationlist_find(list,id);

    if(entry == NULL)  {
        destinationlist_entry_init(&empty);
        if( (r = strbuf_copy(&empty.id,id)) != 0 ) {
            fprintf(stderr,"[destinationlist:configure] error allocating id string\n");
            return r;
        }
        if( (r = membuf_append(list,&empty,sizeof(destinationlist_entry))) != 0) {
            fprintf(stderr,"[destinationlist:configure] error allocating destinationlist_entry\n");
            return r;
        }
        entry = destinationlist_find(list,id);
        if(entry == NULL) abort();
    }

    return destination_config(&entry->destination,key,value);
}

int destinationlist_open(const destinationlist* list, const ich_time* now) {
    int r;
    size_t i;
    size_t len;

    destinationlist_entry* entry = (destinationlist_entry *)list->x;
    len = list->len / sizeof(destinationlist_entry);

    for(i=0;i<len;i++) {
        if( (r = destination_open(&entry[i].destination,now)) != 0) {
            fprintf(stderr,"[destinationlist] error opening destination %.*s\n",
              (int)entry[i].id.len, (char *)entry[i].id.x);
            return r;
        }
    }

    return 0;
}

static int destinationlist_entry_run(void *userdata) {
    int r;
    destinationlist_entry* entry = (destinationlist_entry*)userdata;

    entry->sync.on_frame.cb       = (frame_handler_callback)destination_submit_frame;
    entry->sync.on_frame.flush    = (frame_handler_flush_callback)destination_flush;
    entry->sync.on_frame.userdata = &entry->destination;
    entry->sync.on_tags.cb        = (tag_handler_callback)destination_submit_tags;
    entry->sync.on_tags.userdata  = &entry->destination;
    entry->sync.tagmap            = entry->destination.tagmap;
    entry->sync.map_flags         = &entry->destination.map_flags;

    r = destination_sync_run(&entry->sync);

    thread_exit(r);
    return r;
}

int destinationlist_start(const destinationlist* list) {
    size_t i;
    size_t len;

    destinationlist_entry* entry = (destinationlist_entry *)list->x;
    len = list->len / sizeof(destinationlist_entry);

    for(i=0;i<len;i++) {
        entry[i].thread = thread_create(destinationlist_entry_run, &entry[i], THREAD_STACK_SIZE_DEFAULT);
    }

    return 0;
}

int destinationlist_wait(const destinationlist* list) {
    size_t i;
    size_t len;

    destinationlist_entry* entry = (destinationlist_entry *)list->x;
    len = list->len / sizeof(destinationlist_entry);

    for(i=0;i<len;i++) {
        thread_join(entry[i].thread);
    }

    return 0;
}
