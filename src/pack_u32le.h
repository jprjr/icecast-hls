#ifndef PACK_U32LE_H
#define PACK_U32LE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static void pack_u32le(uint8_t* d, uint32_t n);

#ifdef __cplusplus
}
#endif

static void pack_u32le(uint8_t* d, uint32_t n) {
    d[0] = ( n       ) & 0xFF;
    d[1] = ( n >> 8  ) & 0xFF;
    d[2] = ( n >> 16 ) & 0xFF;
    d[3] = ( n >> 24 ) & 0xFF;
}

#endif

