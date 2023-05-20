#include "sourcelist.h"
#include "source_sync.h"

#include "destination.h"

#include <stdio.h>
#include <stdlib.h>

/* tell all destination threads to just quit, something has gone wrong */
static void sourcelist_entry_quit(sourcelist_entry* entry, int status) {
    size_t i;
    size_t len;
    source_sync sync;

    destination_sync** dest_sync;

    len = entry->destination_syncs.len / sizeof(destination_sync*);
    dest_sync = (destination_sync**)entry->destination_syncs.x;

    for(i=0;i<len;i++) {
        sync.dest = dest_sync[i];
        source_sync_quit(&sync);
    }
    (void) status;
}

size_t sourcelist_length(const sourcelist* slist) {
    return slist->len / sizeof(sourcelist_entry);
}

void sourcelist_init(sourcelist* slist) {
    membuf_init(slist);
}

void sourcelist_entry_init(sourcelist_entry* entry) {
    strbuf_init(&entry->id);
    /* source_init(&entry->source); this gets done after appending the entry */
    membuf_init(&entry->destination_syncs);
    thread_atomic_int_store(&entry->status, 0);
    entry->quit = NULL;
    entry->quit_userdata = NULL;
    entry->samplecount = 0;
}

void sourcelist_entry_dump_counters(const sourcelist_entry* entry) {
    strbuf tmp = STRBUF_ZERO;
    if(strbuf_append_cstr(&tmp,"[source.")) abort();
    if(strbuf_cat(&tmp,&entry->id)) abort();
    if(strbuf_append_cstr(&tmp,"]")) abort();
    source_dump_counters(&entry->source, &tmp);
    strbuf_free(&tmp);
}

void sourcelist_entry_free(sourcelist_entry* entry) {
    strbuf_free(&entry->id);
    source_free(&entry->source);
    membuf_free(&entry->destination_syncs);
}

void sourcelist_free(sourcelist* slist) {
    size_t i;
    size_t len;

    sourcelist_entry* entry = (sourcelist_entry *)slist->x;
    len = slist->len / sizeof(sourcelist_entry);

    for(i=0;i<len;i++) {
        sourcelist_entry_free(&entry[i]);
    }

    membuf_free(slist);
}

sourcelist_entry* sourcelist_find(const sourcelist* list, const strbuf* id) {
    size_t i;
    size_t len;

    sourcelist_entry* entry = (sourcelist_entry *)list->x;
    len = list->len / sizeof(sourcelist_entry);

    for(i=0;i<len;i++) {
        if(strbuf_equals(&entry[i].id,id)) return &entry[i];
    }
    return NULL;
}

sourcelist_entry* sourcelist_get(const sourcelist* list, size_t index) {
    sourcelist_entry* entry = (sourcelist_entry *)list->x;
    if(index < sourcelist_length(list)) {
        return &entry[index];
    }
    return NULL;
}

int sourcelist_configure(const strbuf* id, const strbuf* key, const strbuf* value, sourcelist* list) {
    int r;
    sourcelist_entry empty;
    sourcelist_entry* entry = sourcelist_find(list,id);

    if(entry == NULL)  {
        sourcelist_entry_init(&empty);
        if( (r = strbuf_copy(&empty.id,id)) != 0 ) return r;
        if( (r = membuf_append(list,&empty,sizeof(sourcelist_entry))) != 0) return r;
        entry = sourcelist_find(list,id);
        if(entry == NULL) abort();
        /* need to init some pointers after memcpy */
        source_init(&entry->source);
    }

    return source_config(&entry->source,key,value);
}

int sourcelist_open(const sourcelist* list, uint8_t shortflag) {
    int r;
    size_t i;
    size_t len;

    sourcelist_entry* entry = (sourcelist_entry *)list->x;
    len = list->len / sizeof(sourcelist_entry);

    for(i=0;i<len;i++) {
        if( (r = source_open(&entry[i].source)) != 0) {
            fprintf(stderr,"[sourcelist] error opening source %.*s\n",
              (int)entry[i].id.len, (char *)entry[i].id.x);
            return r;
        }

        if(shortflag) {
            entry[i].quit = (sourcelist_quit_func)sourcelist_quit;
            entry[i].quit_userdata = (void *)list;
        } else {
            entry[i].quit = (sourcelist_quit_func)sourcelist_entry_quit;
            entry[i].quit_userdata = (void *)&entry[i];
        }
    }
    return 0;
}

static int sourcelist_entry_tag_handler(void* userdata, const taglist* tags) {
    int r;
    size_t i;
    size_t len;
    source_sync sync;

    sourcelist_entry* entry = (sourcelist_entry *)userdata;
    destination_sync** dest_sync;

    len = entry->destination_syncs.len / sizeof(destination_sync*);
    dest_sync = (destination_sync**)entry->destination_syncs.x;

    /* if this is set it means somebody called quit, return with the
     * status code given */
    if( (r = thread_atomic_int_load(&entry->status)) != 0) {
        sourcelist_entry_quit(entry,r);
        return r;
    }

    for(i=0;i<len;i++) {
        sync.dest = dest_sync[i];
        if( (r = source_sync_tags(&sync,tags)) != 0) {
            return r;
        }
    }
    return 0;
}

static int sourcelist_entry_frame_handler(void* userdata, const frame* frame) {
    int r;
    size_t i;
    size_t len;
    source_sync sync;
    ich_time now;
    ich_time exp;
    ich_time diff;
    ich_frac frac;


    sourcelist_entry* entry = (sourcelist_entry *)userdata;
    destination_sync** dest_sync;

    if(entry->samplecount == 0) {
        ich_time_now(&entry->ts);
    }
    entry->samplecount += frame->duration;

    if(entry->samplecount >= frame->sample_rate) {
        ich_time_now(&now);
        frac.num = entry->samplecount;
        frac.den = frame->sample_rate;
        exp = entry->ts;
        ich_time_add_frac(&exp,&frac);
        if(ich_time_cmp(&exp,&now) < 0) {/* exp < now, meaning we're behind */
            ich_time_sub(&diff,&now,&exp);
            if(diff.seconds > 0 || diff.nanoseconds > 500000000) {
                fprintf(stderr, "[source.%.*s] [WARNING] audio decoding behind by %ld.%03ld\n",(int)entry->id.len,(const char *)entry->id.x,diff.seconds,diff.nanoseconds / (1000 * 1000));
            }
        }

        entry->samplecount -= frame->sample_rate;
        ich_time_now(&entry->ts);
    }

    len = entry->destination_syncs.len / sizeof(destination_sync*);
    dest_sync = (destination_sync**)entry->destination_syncs.x;

    if( (r = thread_atomic_int_load(&entry->status)) != 0) {
        sourcelist_entry_quit(entry,r);
        return r;
    }

    for(i=0;i<len;i++) {
        sync.dest = dest_sync[i];
        if( (r = source_sync_frame(&sync,frame)) != 0) {
            return r;
        }
    }
    return 0;
}

static int sourcelist_entry_flush_handler(void* userdata) {
    int r;
    size_t i;
    size_t len;
    source_sync sync;

    sourcelist_entry* entry = (sourcelist_entry *)userdata;
    destination_sync** dest_sync;

    len = entry->destination_syncs.len / sizeof(destination_sync*);
    dest_sync = (destination_sync**)entry->destination_syncs.x;

    if( (r = thread_atomic_int_load(&entry->status)) != 0) {
        sourcelist_entry_quit(entry,r);
        return r;
    }

    for(i=0;i<len;i++) {
        sync.dest = dest_sync[i];
        if( (r = source_sync_eof(&sync)) != 0) break;
    }

    return r;
}

static int sourcelist_entry_run(void *userdata) {
    int r = 0;
    sourcelist_entry* entry = (sourcelist_entry *)userdata;

    tag_handler thdlr;
    frame_receiver receiver = FRAME_RECEIVER_ZERO;

    thdlr.cb = sourcelist_entry_tag_handler;
    thdlr.userdata = entry;

    receiver.submit_frame = sourcelist_entry_frame_handler;
    receiver.flush        = sourcelist_entry_flush_handler;
    receiver.handle       = entry;

    entry->source.tag_handler = thdlr;
    entry->source.frame_destination = receiver;

    r = source_run(&entry->source);
    /* (maybe) make all other threads quit */
    entry->quit(entry->quit_userdata,r == 0 ? 1 : -1);

    thread_exit(r);
    return r;
}

int sourcelist_start(const sourcelist* list) {
    size_t i;
    size_t len;

    sourcelist_entry* entry = (sourcelist_entry *)list->x;
    len = list->len / sizeof(sourcelist_entry);

    for(i=0;i<len;i++) {
        entry[i].thread = thread_create(sourcelist_entry_run, &entry[i], THREAD_STACK_SIZE_DEFAULT);
    }

    return 0;
}

int sourcelist_wait(const sourcelist* list) {
    size_t i;
    size_t len;
    int r = 0;

    sourcelist_entry* entry = (sourcelist_entry *)list->x;
    len = list->len / sizeof(sourcelist_entry);

    for(i=0;i<len;i++) {
        if(thread_join(entry[i].thread) < 0) r = -1;
    }

    return r;
}


void sourcelist_quit(const sourcelist* list, int status) {
    size_t i;
    size_t len;

    sourcelist_entry* entry = (sourcelist_entry *)list->x;
    len = list->len / sizeof(sourcelist_entry);

    for(i=0;i<len;i++) {
        thread_atomic_int_store(&entry->status,status);
    }
}

void sourcelist_dump_counters(const sourcelist* list) {
    size_t i;
    size_t len;


    sourcelist_entry* entry = (sourcelist_entry *)list->x;
    len = list->len / sizeof(sourcelist_entry);

    for(i=0;i<len;i++) {
        sourcelist_entry_dump_counters(&entry[i]);
    }

}

