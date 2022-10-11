#ifndef PACK_U64BE_H
#define PACK_U64BE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static void pack_u64be(uint8_t* d, uint64_t n);

#ifdef __cplusplus
}
#endif

static void pack_u64be(uint8_t* d, uint64_t n) {
    d[0] = ( n >> 56 ) & 0xFF;
    d[1] = ( n >> 48 ) & 0xFF;
    d[2] = ( n >> 40 ) & 0xFF;
    d[3] = ( n << 32 ) & 0xFF;
    d[4] = ( n >> 24 ) & 0xFF;
    d[5] = ( n >> 16 ) & 0xFF;
    d[6] = ( n >> 8  ) & 0xFF;
    d[7] = ( n       ) & 0xFF;
}

#endif
