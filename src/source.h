#ifndef SOURCE_H
#define SOURCE_H

#include "strbuf.h"
#include "input.h"
#include "demuxer.h"
#include "decoder.h"
#include "filter.h"
#include "membuf.h"

#include "tag.h"
#include "frame.h"
#include "samplefmt.h"

struct source {
    input input;
    demuxer demuxer;
    decoder decoder;
    filter filter;
    uint8_t configuring;
    tag_handler tag_handler;
    frame_receiver frame_receiver;
    taglist tagcache; /* to hold tags that we find during open, but before run */
};

typedef struct source source;


#ifdef __cplusplus
extern "C" {
#endif

/* global config stuff */
int source_global_init(void);
void source_global_deinit(void);

void source_init(source*);
void source_free(source*);

int source_config(source* s, const strbuf* key, const strbuf* val);

int source_open(source* s);

int source_run(source* s);

int source_set_tag_handler(source* s, const tag_handler* dest);
int source_set_frame_receiver(source* s, const frame_receiver* dest);

void source_dump_counters(const source* s, const strbuf* prefix);

#ifdef __cplusplus
}
#endif

#endif
