#ifndef UNPACK_U16BE_H
#define UNPACK_U16BE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static uint16_t unpack_u16be(const uint8_t* d);

#ifdef __cplusplus
}
#endif

static uint16_t unpack_u16be(const uint8_t* d) {
    return (((uint16_t)d[0])<< 8) |
           (((uint16_t)d[1])    );
}

#endif
