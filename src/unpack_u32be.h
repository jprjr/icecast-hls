#ifndef UNPACK_U32BE_H
#define UNPACK_U32BE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static uint32_t unpack_u32be(const uint8_t* d);

#ifdef __cplusplus
}
#endif

static uint32_t unpack_u32be(const uint8_t* d) {
    return (((uint32_t)d[0])<<24) |
           (((uint32_t)d[1])<<16) |
           (((uint32_t)d[2])<<8 ) |
           (((uint32_t)d[3])    );
}

#endif
