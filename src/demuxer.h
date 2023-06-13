#ifndef DEMUXER_H
#define DEMUXER_H

#include "demuxer_plugin.h"
#include "ich_time.h"

struct demuxer {
    void* userdata;
    const demuxer_plugin* plugin;
    tag_handler tag_handler;
    packet_receiver packet_receiver;
    size_t counter;
    ich_time ts;
};

typedef struct demuxer demuxer;

#ifdef __cplusplus
extern "C" {
#endif

/* performs any needed global init/deinit for demuxer plugins */
int demuxer_global_init(void);
void demuxer_global_deinit(void);

/* performs any startup initialization, returns 0 on success */
void demuxer_init(demuxer* dec);

/* close out an demuxer, free any resources */
void demuxer_free(demuxer* dec);

int demuxer_create(demuxer *dec, const strbuf* plugin_name);

int demuxer_config(const demuxer* dec, const strbuf* name, const strbuf* value);

/* try to open the demuxer */
int demuxer_open(demuxer* dec, input* in);

/* runs the demuxer plugin for 1 packet of audio, submitting any
 * metadata it finds as it goes, returns 1 on EOF, -1 on error */
int demuxer_run(demuxer* dec);

void demuxer_dump_counters(const demuxer* dec, const strbuf* prefix);

#ifdef __cplusplus
}
#endif

#endif

