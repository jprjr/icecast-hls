#ifndef UNPACK_U32LE_H
#define UNPACK_U32LE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static uint32_t unpack_u32le(const uint8_t* d);

#ifdef __cplusplus
}
#endif

static uint32_t unpack_u32le(const uint8_t* d) {
    return (((uint32_t)d[0])    ) |
           (((uint32_t)d[1])<< 8) |
           (((uint32_t)d[2])<<16) |
           (((uint32_t)d[3])<<24);
}

#endif
