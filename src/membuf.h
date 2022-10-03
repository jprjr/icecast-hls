#ifndef MEMBUF_H
#define MEMBUF_H

#include <stddef.h>
#include <stdint.h>

/* a dynamic memory buffer that
 * can be used for, y'know, anything */

struct membuf {
    size_t a;
    size_t len;
    size_t blocksize;
    uint8_t* x;
};

typedef struct membuf membuf;

#define MEMBUF_ZERO { .a = 0, .len = 0, .blocksize = 512, .x = NULL }

#ifdef __cplusplus
extern "C" {
#endif

void membuf_init(membuf*);
void membuf_init_bs(membuf*,size_t);
void membuf_free(membuf*);
void membuf_reset(membuf*);

int membuf_ready(membuf*, size_t len);
int membuf_readyplus(membuf*, size_t len);

int membuf_insert(membuf*, const void* src, size_t len, size_t pos);
int membuf_append(membuf*, const void *src, size_t len);
int membuf_prepend(membuf*, const void *src, size_t len);
int membuf_remove(membuf*, size_t len, size_t pos);

/* discard removes bytes from end of buffer */
int membuf_discard(membuf*, size_t len);

/* trim removes bytes from beginning of buffer */
int membuf_discard(membuf*, size_t len);

int membuf_copy(membuf*, const membuf* s);
int membuf_cat(membuf*, const membuf* s);

#ifdef __cplusplus
}
#endif

#endif
