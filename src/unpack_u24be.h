#ifndef UNPACK_U24BE_H
#define UNPACK_U24BE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static uint32_t unpack_u24be(const uint8_t* d);

#ifdef __cplusplus
}
#endif

static uint32_t unpack_u24be(const uint8_t* d) {
    return (((uint32_t)d[0])<<16) |
           (((uint32_t)d[1])<<8 ) |
           (((uint32_t)d[2])    );
}

#endif

