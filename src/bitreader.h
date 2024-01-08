#ifndef BITREADER_H
#define BITREADER_H

#include <stdint.h>
#include <assert.h>

struct bitreader {
    uint64_t val;
    uint8_t  bits;
    size_t   pos;
    size_t   len;
    const uint8_t* buffer;
};

typedef struct bitreader bitreader;

#define BITREADER_ZERO { .val = 0, .bits = 0, .pos = 0, .len = 0, .buffer = NULL }
static const bitreader bitreader_zero = BITREADER_ZERO;

static inline void
bitreader_init(bitreader* br) {
    *br = bitreader_zero;
}

static inline int
bitreader_fill(bitreader* br, uint8_t bits);

static inline uint64_t
bitreader_read(bitreader* br, uint8_t bits);

static inline void
bitreader_discard(bitreader* br, uint8_t bits);

static inline uint64_t
bitreader_peek(bitreader* br, uint8_t bits);

static inline void
bitreader_align(bitreader* br);

static inline int
bitreader_fill(bitreader* br, uint8_t bits) {
    uint8_t byte = 0;
    assert(bits <= 64);
    if(bits == 0) return 0;
    while(br->bits < bits && br->pos < br->len) {
        byte = br->buffer[br->pos++];
        br->val = (br->val << 8) | byte;
        br->bits += 8;
    }
    return br->bits < bits;
}


static inline uint64_t
bitreader_read(bitreader* br, uint8_t bits) {
    uint64_t mask = -1LL;
    uint64_t imask = -1LL;
    uint64_t r;

    if(bits > br->bits) bitreader_fill(br,bits - br->bits);
    assert(bits <= br->bits);

    if(bits == 0) return 0;

    mask >>= (64 - bits);
    br->bits -= bits;
    r = br->val >> br->bits & mask;
    if(br->bits == 0) {
        imask = 0;
    } else {
        imask >>= (64 - br->bits);
    }
    br->val &= imask;

    return r;
}


static inline uint64_t
bitreader_peek(bitreader* br, uint8_t bits) {
    uint64_t mask = -1LL;
    uint64_t r;

    if(bits > br->bits) bitreader_fill(br,bits - br->bits);
    assert(bits <= br->bits);
    if(bits == 0) return 0;

    mask >>= (64 - bits);
    r = br->val >> (br->bits - bits) & mask;
    return r;
}

static inline void
bitreader_discard(bitreader* br, uint8_t bits) {
    uint64_t imask = -1LL;

    if(bits > br->bits) bitreader_fill(br,bits - br->bits);
    assert(bits <= br->bits);
    if(bits == 0) return;

    br->bits -= bits;

    if(br->bits == 0) {
        imask = 0;
    } else {
        imask >>= (64 - br->bits);
    }
    br->val &= imask;
}

static inline void
bitreader_align(bitreader* br) {
    assert(br->bits < 8);
    br->bits = 0;
    br->val = 0;
}

#endif
