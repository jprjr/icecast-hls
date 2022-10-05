#ifndef UNPACK_U16LE_H
#define UNPACK_U16LE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static uint16_t unpack_u16le(const uint8_t* d);

#ifdef __cplusplus
}
#endif

static uint16_t unpack_u16le(const uint8_t* d) {
    return (((uint16_t)d[0])    ) |
           (((uint16_t)d[1])<< 8);
}

#endif
