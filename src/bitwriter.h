#ifndef BITWRITER_H
#define BITWRITER_H

#include <stddef.h>
#include <stdint.h>

struct bitwriter {
    uint64_t val;
    uint8_t  bits;
    size_t   pos;
    size_t   len;
    uint8_t* buffer;
};

typedef struct bitwriter bitwriter;

#define BITWRITER_ZERO { .val = 0, .bits = 0, .pos = 0, .len = 0, .buffer = NULL }
static const bitwriter bitwriter_zero = BITWRITER_ZERO;

static inline void bitwriter_init(bitwriter* bw) {
    *bw = bitwriter_zero;
}

static inline void bitwriter_flush(bitwriter* bw) {
    uint64_t avail = ((bw->len - bw->pos) * 8);
    uint64_t mask  = -1LL;
    uint8_t  byte  = 0;

    while(avail && bw->bits > 7) {
        bw->bits -= 8;
        byte = (uint8_t)((bw->val >> bw->bits) & 0xFF);
        bw->buffer[bw->pos++] = byte;
        avail -= 8;
    }
    if(bw->bits == 0) {
        bw->val = 0;
    } else {
        mask >>= 64 - bw->bits;
        bw->val &= mask;
    }
}

static int bitwriter_add(bitwriter *bw, uint8_t bits, uint64_t val) {
    uint64_t mask  = -1LL;
    if(bw->bits + bits > 64) bitwriter_flush(bw);

    mask >>= (64 - bits);
    bw->val <<= bits;
    bw->val |= val & mask;
    bw->bits += bits;
    return 1;
}

static void bitwriter_align(bitwriter *bw) {
    uint8_t r = bw->bits % 8;
    if(r) {
        bitwriter_add(bw,8-r,0);
    }
    bitwriter_flush(bw);
}

#endif
