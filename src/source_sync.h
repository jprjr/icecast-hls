#ifndef SOURCE_SYNC_H
#define SOURCE_SYNC_H

#include "destination_sync.h"

struct source_sync {
    destination_sync *dest;
};

typedef struct source_sync source_sync;

#ifdef __cplusplus
extern "C" {
#endif

void source_sync_init(source_sync*);
void source_sync_free(source_sync*);

/* all these void*s are cast into source_sync */
int source_sync_frame(source_sync*, const frame* frame);
int source_sync_tags(source_sync*, const taglist* tags);
void source_sync_eof(source_sync*);

#ifdef __cplusplus
}
#endif

#endif
