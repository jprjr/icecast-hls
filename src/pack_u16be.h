#ifndef PACK_U16BE_H
#define PACK_U16BE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static void pack_u16be(uint8_t* d, uint16_t n);

#ifdef __cplusplus
}
#endif

static void pack_u16be(uint8_t* d, uint16_t n) {
    d[0] = ( n >> 8  ) & 0xFF;
    d[1] = ( n       ) & 0xFF;
}

#endif
