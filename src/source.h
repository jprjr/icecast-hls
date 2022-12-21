#ifndef SOURCE_H
#define SOURCE_H

#include "strbuf.h"
#include "input.h"
#include "decoder.h"
#include "filter.h"
#include "membuf.h"

#include "tag.h"
#include "frame.h"
#include "samplefmt.h"
#include "ich_time.h"

struct source {
    input input;
    decoder decoder;
    filter filter;
    uint8_t configuring;
    tag_handler tag_handler;
    frame_receiver frame_destination;
    taglist tagcache; /* to hold tags that we find during open, but before run */
    frame_source frame_source; /* during open, the decoder/filter will "open" on a dummy
    destination that just copies the source info, which is used during source_open_dest */
    size_t samplecount; /* counts number of samples seen */
    ich_time ts; /* timestamp to track datarate */
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

/* unlike the decoders etc, during open audioconfig is *not* submitted,
 * that gets delayed until we start opening destinations, the destination will
 * call this to get the pipeline ready */
int source_open_dest(const source* s, const frame_receiver* dest);

int source_run(const source* s);

int source_set_tag_handler(source* s, const tag_handler* dest);
int source_set_frame_receiver(source* s, const frame_receiver* dest);

#ifdef __cplusplus
}
#endif

#endif
