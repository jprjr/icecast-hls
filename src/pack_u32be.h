#ifndef PACK_U32BE_H
#define PACK_U32BE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static void pack_u32be(uint8_t* d, uint32_t n);

#ifdef __cplusplus
}
#endif

static void pack_u32be(uint8_t* d, uint32_t n) {
    d[0] = ( n >> 24 ) & 0xFF;
    d[1] = ( n >> 16 ) & 0xFF;
    d[2] = ( n >> 8  ) & 0xFF;
    d[3] = ( n       ) & 0xFF;
}

#endif
