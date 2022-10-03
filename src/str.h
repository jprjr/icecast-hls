#ifndef STR_H
#define STR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int str_begins(const char *s, const char *q);
int str_ends(const char *s, const char *q);

#ifdef __cplusplus
}
#endif

#endif
