#ifndef TAGMAP_DEFAULT_H
#define TAGMAP_DEFAULT_H

#include "tag.h"

extern taglist _DEFAULT_TAGMAP;
extern taglist* DEFAULT_TAGMAP;

#ifdef __cplusplus
extern "C" {
#endif

int default_tagmap_init(void);
void default_tagmap_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
