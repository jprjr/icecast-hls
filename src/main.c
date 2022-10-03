#include "source.h"
#include "destination.h"

#include "sourcelist.h"
#include "destinationlist.h"
#include "tagmap.h"
#include "tagmap_default.h"
#include "ich_time.h"

#include "ini.h"

#include <stdio.h>
#include <string.h>

struct app_config {
    uint8_t shortflag;
    sourcelist* slist;
    destinationlist* dlist;
    tagmap* tagmap;
    ich_time* now;
};

typedef struct app_config app_config;

void app_config_init(app_config* config) {
    config->shortflag = 1; /* default is to stop all other streams on a source ending */
    sourcelist_init(config->slist);
    destinationlist_init(config->dlist);
    tagmap_init(config->tagmap);
}

void app_config_free(app_config* config) {
    sourcelist_free(config->slist);
    destinationlist_free(config->dlist);
    tagmap_free(config->tagmap);
}

static int config_handler(void* user, const char* section, const char* name, const char* value) {
    app_config* config = (app_config*)user;

    strbuf section_buf;
    strbuf name_buf;
    strbuf value_buf;

    section_buf.len = strlen(section);
    section_buf.x   = (uint8_t *)section;
    section_buf.a   = 0;

    name_buf.len = strlen(name);
    name_buf.x   = (uint8_t *)name;
    name_buf.a   = 0;

    value_buf.len = strlen(value);
    value_buf.x   = (uint8_t *)value;
    value_buf.a   = 0;

    if(strbuf_begins_cstr(&section_buf,"source.")) {
        if(section_buf.len < strlen("source.") + 1) {
            fprintf(stderr,"invalid section id\n");
            return 1;
        }
        section_buf.x = &section_buf.x[strlen("source.")];
        section_buf.len -= strlen("source.");
        return sourcelist_configure(&section_buf, &name_buf, &value_buf, config->slist) == 0;
    }

    if(strbuf_begins_cstr(&section_buf,"destination.")) {
        if(section_buf.len < strlen("destination.") + 1) {
            fprintf(stderr,"invalid section id\n");
            return 1;
        }
        section_buf.x = &section_buf.x[strlen("destination.")];
        section_buf.len -= strlen("destination.");
        return destinationlist_configure(&section_buf, &name_buf, &value_buf, config->dlist) == 0;
    }

    if(strbuf_begins_cstr(&section_buf,"tagmap.")) {
        if(section_buf.len < strlen("tagmap.") + 1) {
            fprintf(stderr,"invalid section id\n");
            return 1;
        }
        section_buf.x = &section_buf.x[strlen("tagmap.")];
        section_buf.len -= strlen("tagmap.");
        return tagmap_configure(&section_buf, &name_buf, &value_buf, config->tagmap) == 0;
    }

    if(strbuf_equals_cstr(&section_buf,"options")) {
        if(strbuf_equals_cstr(&name_buf,"stop-on-source-ending")) {
            if(strbuf_truthy(&value_buf)) {
                config->shortflag = 1;
                return 1;
            }
            if(strbuf_falsey(&value_buf)) {
                config->shortflag = 0;
                return 1;
            }
            fprintf(stderr,"[config] section %s: unknown value %s for option %s\n",section, value,name);
            return 0;
        }
        fprintf(stderr,"[config] section %s: unknown option %s\n",section,name);
        return 0;
    }

    fprintf(stderr,"[config] unknown section %s\n", section);
    return 0;
}

static void prep_tagmaps(tagmap* maps) {
    size_t i = 0;
    size_t len = tagmap_len(maps);
    tagmap_entry* e = NULL;
    for(i=0;i<len;i++) {
        e = tagmap_get(maps,i);
        taglist_sort(&e->map);
    }
}


static int link_destinations(sourcelist* slist, destinationlist* dlist, tagmap *maps) {
    int r;
    size_t i = 0;
    size_t len = destinationlist_length(dlist);

    sourcelist_entry* se;
    destinationlist_entry* de;
    destination_sync* sync;
    tagmap_entry* map_entry;

    for(i=0;i<len;i++) {
        de = destinationlist_get(dlist,i);
        if(de->destination.source_id.len == 0) {
            fprintf(stderr,"error: destination %.*s has no source configured\n",
            (int)de->id.len,(char *)de->id.x);
        }
        se = sourcelist_find(slist,&de->destination.source_id);
        if(se == NULL) {
            fprintf(stderr,"error: destination %.*s using source %.*s, which doesn't exist\n",
            (int)de->id.len,(char *)de->id.x,
            (int)de->destination.source_id.len,(char *)de->destination.source_id.x);
            return -1;
        }
        de->destination.source = &se->source;

        sync = &de->sync;
        if( (r = membuf_append(&se->destination_syncs,&sync,sizeof(destination_sync*))) != 0) {
            fprintf(stderr,"error linking source and destination: out of memory\n");
            return -1;
        }
        if(de->destination.tagmap_id.len == 0) {
            de->destination.tagmap = DEFAULT_TAGMAP;
            continue;
        }
        map_entry = tagmap_find(maps,&de->destination.tagmap_id);
        if(map_entry == NULL) {
            fprintf(stderr,"error: destination %.*s using tagmap %.*s, which doesn't exist\n",
            (int)de->id.len,(char *)de->id.x,
            (int)de->destination.tagmap_id.len,(char *)de->destination.tagmap_id.x);
            return -1;
        }
        de->destination.tagmap = &map_entry->map;
    }

    len = sourcelist_length(slist);
    for(i=0;i<len;i++) {
        se = sourcelist_get(slist,i);
        if(se->destination_syncs.len == 0) {
            fprintf(stderr,"error: source %.*s has no destinations\n",
            (int)se->id.len,(char *)se->id.x);
            return -1;
        }
    }
    return 0;
}

int main(int argc, const char* argv[]) {
    int r;
    int ret = 1;
    app_config config;

    sourcelist slist;
    destinationlist dlist;
    tagmap tagmap;
    ich_time now;

    config.slist = &slist;
    config.dlist = &dlist;
    config.tagmap = &tagmap;
    config.now = &now;

    ich_time_now(&now);

    app_config_init(&config);

    if( (r = source_global_init()) != 0) {
        fprintf(stderr,"error initializing source plugins\n");
        return r;
    }

    if( (r = destination_global_init()) != 0) {
        fprintf(stderr,"error initializing destination plugins\n");
        return r;
    }

    if( (r = default_tagmap_init()) != 0) {
        fprintf(stderr,"error initializing default tag mapping\n");
        return r;
    }

    r = ini_parse(argv[1],config_handler,&config);
    if(r != 0) {
        printf("Parsing INI experienced error on line %d\n", r);
        goto cleanup;
    }

    prep_tagmaps(&tagmap);

    r = link_destinations(&slist,&dlist,&tagmap);
    if(r != 0) {
        goto cleanup;
    }

    if( (r = sourcelist_open(&slist,config.shortflag)) != 0) {
        fprintf(stderr,"[main] error opening a source\n");
        goto cleanup;
    }

    if( (r = destinationlist_open(&dlist,&now)) != 0) {
        fprintf(stderr,"[main] error opening a source\n");
        goto cleanup;
    }

    destinationlist_start(&dlist);
    sourcelist_start(&slist);
    ret = sourcelist_wait(&slist) != 0;
    destinationlist_wait(&dlist);

    cleanup:
    app_config_free(&config);
    source_global_deinit();
    destination_global_deinit();
    default_tagmap_deinit();
    return ret;
}
