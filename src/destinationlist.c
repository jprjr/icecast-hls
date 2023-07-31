#include "destinationlist.h"
#include <stdlib.h>

#include "logger.h"

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
        logger_set_prefix("destination.",12);
        logger_append_prefix((const char *)entry[i].id.x, entry[i].id.len);
        logger_set_level((enum LOG_LEVEL) (entry[i].loglevel == -1 ? 
          logger_get_default_level() : (enum LOG_LEVEL)entry[i].loglevel));
        destinationlist_entry_free(&entry[i]);
    }

    membuf_free(dlist);
}

void destinationlist_entry_init(destinationlist_entry* entry) {
    strbuf_init(&entry->id);
    destination_sync_init(&entry->sync);
    destination_init(&entry->destination);
    entry->loglevel = -1;
}

void destinationlist_entry_free(destinationlist_entry* entry) {
    strbuf_free(&entry->id);
    destination_sync_free(&entry->sync);
    destination_free(&entry->destination);
}

void destinationlist_entry_dump_counters(const destinationlist_entry* entry) {
    strbuf tmp = STRBUF_ZERO;
    if(strbuf_append_cstr(&tmp,"[destination.")) abort();
    if(strbuf_cat(&tmp,&entry->id)) abort();
    if(strbuf_append_cstr(&tmp,"]")) abort();
    destination_dump_counters(&entry->destination, &tmp);
    strbuf_free(&tmp);
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

    logger_set_prefix("destination.",12);
    logger_append_prefix((const char *)entry->id.x, entry->id.len);

    if(strbuf_equals_cstr(key,"loglevel") ||
       strbuf_equals_cstr(key,"log-level") ||
       strbuf_equals_cstr(key,"log level")) {
        if(strbuf_caseequals_cstr(value,"trace")) {
            entry->loglevel = LOG_TRACE;
            return 0;
        }
        if(strbuf_caseequals_cstr(value,"debug")) {
            entry->loglevel = LOG_DEBUG;
            return 0;
        }
        if(strbuf_caseequals_cstr(value,"info")) {
            entry->loglevel = LOG_INFO;
            return 0;
        }
        if(strbuf_caseequals_cstr(value,"warn")) {
            entry->loglevel = LOG_WARN;
            return 0;
        }
        if(strbuf_caseequals_cstr(value,"error")) {
            entry->loglevel = LOG_ERROR;
            return 0;
        }
        if(strbuf_caseequals_cstr(value,"fatal")) {
            entry->loglevel = LOG_FATAL;
            return 0;
        }

        fprintf(stderr,"unknown value %.*s for option %.*s\n",(int)value->len,value->x,
          (int)key->len,key->x);
        return 1;
    }

    logger_set_level((enum LOG_LEVEL) (entry->loglevel == -1 ? 
      logger_get_default_level() : (enum LOG_LEVEL)entry->loglevel));

    return destination_config(&entry->destination,key,value);
}

int destinationlist_open(const destinationlist* list, const ich_time* now) {
    int r;
    size_t i;
    size_t len;

    destinationlist_entry* entry = (destinationlist_entry *)list->x;
    len = list->len / sizeof(destinationlist_entry);

    for(i=0;i<len;i++) {
        logger_set_prefix("destination.",12);
        logger_append_prefix((const char *)entry[i].id.x, entry[i].id.len);
        logger_set_level((enum LOG_LEVEL) (entry[i].loglevel == -1 ? 
          logger_get_default_level() : (enum LOG_LEVEL)entry[i].loglevel));

        if( (r = destination_create(&entry[i].destination,now)) != 0) {
            fprintf(stderr,"[destinationlist] error prepping destination %.*s\n",
              (int)entry[i].id.len, (char *)entry[i].id.x);
            return r;
        }
    }

    return 0;
}

static int destinationlist_entry_run(void *userdata) {
    int r;
    destinationlist_entry* entry = (destinationlist_entry*)userdata;

    logger_set_prefix("destination.",12);
    logger_append_prefix((const char *)entry->id.x,entry->id.len);
    logger_set_level((enum LOG_LEVEL) (entry->loglevel == -1 ?
      logger_get_default_level() : (enum LOG_LEVEL)entry->loglevel));

    entry->sync.frame_receiver.open         = (frame_receiver_open_cb)destination_open;
    entry->sync.frame_receiver.submit_frame = (frame_receiver_submit_frame_cb)destination_submit_frame;
    entry->sync.frame_receiver.flush        = (frame_receiver_flush_cb)destination_flush;
    entry->sync.frame_receiver.reset        = (frame_receiver_reset_cb)destination_reset;
    entry->sync.frame_receiver.close        = (frame_receiver_close_cb)destination_close;
    entry->sync.frame_receiver.handle       = &entry->destination;
    entry->sync.on_tags.cb        = (tag_handler_callback)destination_submit_tags;
    entry->sync.on_tags.userdata  = &entry->destination;
    entry->sync.tagmap            = entry->destination.tagmap;
    entry->sync.map_flags         = &entry->destination.map_flags;

    r = destination_sync_run(&entry->sync);

    logger_thread_cleanup();
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

void destinationlist_dump_counters(const destinationlist* list) {
    size_t i;
    size_t len;


    destinationlist_entry* entry = (destinationlist_entry *)list->x;
    len = list->len / sizeof(destinationlist_entry);

    for(i=0;i<len;i++) {
        destinationlist_entry_dump_counters(&entry[i]);
    }

}
