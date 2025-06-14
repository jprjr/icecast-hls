#ifndef ID3_H
#define ID3_H

#include "membuf.h"
#include "tag.h"

typedef membuf id3;

#ifdef __cplusplus
extern "C" {
#endif

void id3_init(id3*);
void id3_reset(id3*);
void id3_free(id3*);

int id3_ready(id3*);
int id3_add_tag(id3*, const tag*);
int id3_add_taglist(id3*, const taglist*);

#ifdef __cplusplus
}
#endif

#endif

