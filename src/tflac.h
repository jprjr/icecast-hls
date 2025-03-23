/* SPDX-License-Identifier: 0BSD */

#ifndef TFLAC_HEADER_GUARD
#define TFLAC_HEADER_GUARD

/*
Copyright (C) 2024 John Regan <john@jrjrtech.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
*/



/*

TFLAC
=====

A single-header FLAC encoder that doesn't use the standard library
and doesn't allocate memory (it makes you do it instead!).

Building
========

In one C file, define TFLAC_IMPLEMENTATION before including this header, like:

    #define TFLAC_IMPLEMENTATION
    #include "tflac.h"

You'll need to allocate a tflac, initialize it with tflac_init,
then set the blocksize, bitdepth, channels, and samplerate fields.

    tflac t;

    tflac_init(&t);
    t.channels = 2;
    t.blocksize = 1152;
    t.bitdepth = 16;
    t.samplerate = 44100;

You'll need to allocate a block of memory for tflac to use for residuals calcuation.
You can get the needed memory size with either the TFLAC_SIZE_MEMORY macro or tflac_size_memory()
function. Both versions just require a blocksize.

Then call tflac_validate with your memory block and the size, it will validate that all
your fields make sense and record pointers into the memory block.

    tflac_u32 mem_size = tflac_size_memory(1152);
    void* block = malloc(mem_size);
    tflac_validate(&t, block, mem_size);

You'll also need to allocate a block of memory for storing your encoded
audio. You can find this size with wither the TFLAC_SIZE_FRAME macro
or tflac_size_frame() function. Both versions need the blocksize,
channels, and bitdepth, and return the maximum amount of bytes required
to hold a frame of audio.

    tflac_u32 buffer_len = tflac_size_frame(1152, 2, 16);
    void* buffer = malloc(buffer_len);

Finally to encode audio, you'll call the appropriate tflac_encode function,
depending on how your source data is laid out:
    - tflac_s32** (planar)      => tflac_encode_s32p
    - tflac_s32*  (interleaved) => tflac_encode_s32i
    - tflac_s16** (planar)      => tflac_encode_s16p
    - tflac_s16*  (interleaved) => tflac_encode_s16i

You'll also provide the current block size (all blocks except the last need to
use the same block size), a buffer to encode data to, and your buffer's length.

    tflac_s32* samples[2];
    tflac_u32 used = 0;
    samples[0] = { 0, 1,  2,  3,  4,  5,  6,  7 };
    samples[1] = { 8, 9, 10, 11, 12, 13, 14, 15 };
    tflac_encode_int32p(&t, 8, samples, buffer, buffer_len, &used);

You can now write out used bytes of buffer.

The library also has a convenience function for creating a STREAMINFO block:

    tflac_streaminfo(&t, 1,  buffer, bufferlen, &used);

The first argument is whether to set the "is-last-block" flag.

Other metadata blocks are out-of-scope, as is writing the "fLaC" stream marker.
*/

#ifndef TFLAC_CONST
#if defined(__GNUC__) && __GNUC__ > 3 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 5)
#define TFLAC_CONST __attribute__((__const__))
#else
#define TFLAC_CONST
#endif
#endif

#ifndef TFLAC_PURE
#if defined(__GNUC__) && __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)
#define TFLAC_PURE __attribute__((__pure__))
#else
#define TFLAC_PURE
#endif
#endif

#ifndef TFLAC_RESTRICT
#if __STDC_VERSION__ >= 199901L
#define TFLAC_RESTRICT restrict
#elif defined(__GNUC__) && __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)
#define TFLAC_RESTRICT __restrict
#elif defined(__GNUC__) && __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)
#define TFLAC_RESTRICT __restrict
#elif defined(_MSC_VER) && _MSC_VER >= 1400
#define TFLAC_RESTRICT __restrict
#else
#define TFLAC_RESTRICT
#endif
#endif


#ifndef TFLAC_PUBLIC
#define TFLAC_PUBLIC
#endif

#ifndef TFLAC_PRIVATE
#define TFLAC_PRIVATE static
#endif

#define TFLAC_SIZE_STREAMINFO 38UL
#define TFLAC_SIZE_FRAME(blocksize, channels, bitdepth) \
    (UINT32_C(18) + UINT32_C(8) + UINT32_C(channels) + \
      (( \
        (UINT32_C(blocksize) * UINT32_C(bitdepth) * (UINT32_C(channels) * (UINT32_C(channels) != (UINT32_C(2))))) + \
        (UINT32_C(blocksize) * UINT32_C(bitdepth) * (UINT32_C(channels) == UINT32_C(2))) + \
        (UINT32_C(blocksize) * (UINT32_C(bitdepth) + (UINT32_C(bitdepth) != UINT32_C(32))) * (UINT32_C(channels) == UINT32_C(2))) + \
        7 \
      ) / 8) )

#define TFLAC_SIZE_MEMORY(blocksize) (15UL + (5UL * ((15UL + (UINT32_C(blocksize) * 4UL)) & UINT32_C(0xFFFFFFF0))))


#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <limits.h>

#if (__STDC_VERSION__ >= 199901L) || defined(__GNUC__)
#include <stdint.h>
typedef uint8_t   tflac_u8;
typedef  int8_t   tflac_s8;
typedef uint16_t  tflac_u16;
typedef  int16_t  tflac_s16;
typedef uint32_t  tflac_u32;
typedef  int32_t  tflac_s32;
typedef uintptr_t tflac_uptr;
#ifndef TFLAC_32BIT_ONLY
typedef uint64_t  tflac_u64;
typedef  int64_t  tflac_s64;
#endif
#else

#ifndef UINT8_MAX
#define UINT8_MAX 0xFF
#endif

#ifndef INT8_MAX
#define INT8_MAX 0x7F
#endif

#ifndef UINT16_MAX
#define UINT16_MAX 0xFFFF
#endif

#ifndef INT16_MAX
#define INT16_MAX 0x7FFF
#endif

#ifndef UINT32_MAX
#define UINT32_MAX 0xFFFFFFFFUL
#endif

#ifndef INT32_MAX
#define INT32_MAX 0x7FFFFFFFL
#endif

#if (UINT8_MAX == UCHAR_MAX)
typedef unsigned char tflac_u8;
#ifndef UINT8_C
#define UINT8_C(x) ((tflac_u8)x)
#endif
#else
#error "not supported"
#endif

#if (INT8_MAX == SCHAR_MAX)
typedef signed char tflac_s8;
#ifndef INT8_C
#define INT8_C(x) ((tflac_s8)x)
#endif
#ifndef INT8_MIN
#define INT8_MIN SCHAR_MIN
#endif
#else
#error "not supported"
#endif

#if (UINT16_MAX == USHRT_MAX)
typedef unsigned short tflac_u16;
#ifndef UINT8_C
#define UINT8_C(x) ((tflac_u8)x)
#endif
#else
#error "not supported"
#endif

#if (INT16_MAX == SHRT_MAX)
typedef signed short tflac_s16;
#ifndef INT16_C
#define INT16_C(x) ((tflac_s16)x)
#ifndef INT16_MIN
#define INT16_MIN SHRT_MIN
#endif
#endif
#else
#error "not supported"
#endif

#if (UINT32_MAX == UINT_MAX)
typedef unsigned int tflac_u32;
#ifndef UINT32_C
#define UINT32_C(x) (x ## U)
#endif
#elif (UINT32_MAX == ULONG_MAX)
typedef unsigned long tflac_u32;
#ifndef UINT32_C
#define UINT32_C(x) (x ## UL)
#endif
#else
#error "not supported"
#endif

#if (INT32_MAX == INT_MAX)
typedef signed int tflac_s32;
#ifndef INT32_C
#define INT32_C(x) (x)
#endif
#ifndef INT32_MIN
#define INT32_MIN INT_MIN
#endif
#elif (INT32_MAX == LONG_MAX)
typedef signed long tflac_s32;
#ifndef INT32_C
#define INT32_C(x) (x ## L)
#endif
#ifndef INT32_MIN
#define INT32_MIN LONG_MIN
#endif
#else
#error "not supported"
#endif

#ifdef _MSC_VER
#ifndef TFLAC_32BIT_ONLY
typedef unsigned __int64  tflac_u64;
typedef signed   __int64  tflac_s64;
#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFui64
#endif

#ifndef INT64_MAX
#define INT64_MAX 0x7FFFFFFFFFFFFFFFi64
#endif

#ifndef UINT64_C
#define UINT64_C(x) (x ## ui64)
#endif

#ifndef INT64_C
#define INT64_C(x) (x ## i64)
#endif
#endif

#if !defined(_UINTPTR_T_DEFINED)
/* this only occurs with MSVC < 8.0 which
 * was the first to support 64-bit mode, so
 * we'll just assume 32-bit uintptr_t */
typedef tflac_u32 tflac_uptr;
#else
typedef uintptr_t tflac_uptr;
#endif

#else
/* we're on an unknown platform! */
#define TFLAC_32BIT_ONLY
typedef tflac_u32 tflac_uptr;
#endif

#endif /* end no stdint.h */

#ifdef TFLAC_32BIT_ONLY
struct tflac_u64 {
    tflac_u32 lo;
    tflac_u32 hi;
};
typedef struct tflac_u64 tflac_u64;
typedef tflac_u64 tflac_s64;

typedef tflac_u32 tflac_uint;
#define tflac_pack_uintbe tflac_pack_u32be
#define tflac_pack_uintle tflac_pack_u32le
#else
typedef tflac_u64 tflac_uint;
#define tflac_pack_uintbe tflac_pack_u64be
#define tflac_pack_uintle tflac_pack_u64le
#endif

enum TFLAC_SUBFRAME_TYPE {
    TFLAC_SUBFRAME_CONSTANT   = 0,
    TFLAC_SUBFRAME_VERBATIM   = 1,
    TFLAC_SUBFRAME_FIXED      = 2,
    TFLAC_SUBFRAME_LPC        = 3,
    TFLAC_SUBFRAME_TYPE_COUNT = 4
};

typedef enum TFLAC_SUBFRAME_TYPE TFLAC_SUBFRAME_TYPE;

enum TFLAC_CHANNEL_MODE {
    TFLAC_CHANNEL_INDEPENDENT  = 0,
    TFLAC_CHANNEL_LEFT_SIDE    = 1,
    TFLAC_CHANNEL_SIDE_RIGHT   = 2,
    TFLAC_CHANNEL_MID_SIDE     = 3,
    TFLAC_CHANNEL_MODE_COUNT   = 4,
};

typedef enum TFLAC_CHANNEL_MODE TFLAC_CHANNEL_MODE;

struct tflac_bitwriter {
    tflac_uint val;
    tflac_u32  bits;
    tflac_u32  pos;
    tflac_u32  len;
    tflac_u32  tot;
    tflac_u8*  buffer;
};

typedef struct tflac_bitwriter tflac_bitwriter;

struct tflac_md5 {
    tflac_u32 a;
    tflac_u32 b;
    tflac_u32 c;
    tflac_u32 d;
    tflac_u32 pos;
    tflac_u64 total;
    tflac_u8 buffer[64+8];
};

typedef struct tflac_md5 tflac_md5;

struct tflac {
    tflac_bitwriter bw;
    tflac_md5 md5_ctx;
    tflac_u32 blocksize;
    tflac_u32 samplerate;
    tflac_u32 channels;
    tflac_u32 bitdepth;

    tflac_u8 channel_mode;
    tflac_u8 max_rice_value; /* defaults to 14 if bitdepth < 16; 30 otherwise */
    tflac_u8 min_partition_order; /* defaults to 0 */
    tflac_u8 max_partition_order; /* defaults to 0, should be <=8 to be in streamable subset */
    tflac_u8 partition_order;

    tflac_u8 enable_constant_subframe;
    tflac_u8 enable_fixed_subframe;
    tflac_u8 enable_md5;

    tflac_u32 frame_header;

    tflac_u64 samplecount;
    tflac_u32 frameno;
    tflac_u32 verbatim_subframe_bits;
    tflac_u32 cur_blocksize;
    tflac_u32 max_frame_len; /* stores the max allowed frame length */

    tflac_u32 min_frame_size;
    tflac_u32 max_frame_size;

    tflac_u32 wasted_bits;
    tflac_u32 subframe_bitdepth;
    tflac_u8 constant;

    tflac_u8 md5_digest[16];

    void (*calculate_order[5])(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT,
      tflac_s32* TFLAC_RESTRICT,
      tflac_u64* TFLAC_RESTRICT
    );

    tflac_u64 residual_errors[5];
    tflac_s32* residuals[5]; /* orders 0, 1, 2, 3, 4 */

#ifndef TFLAC_DISABLE_COUNTERS
    tflac_u64 subframe_type_counts[8][TFLAC_SUBFRAME_TYPE_COUNT]; /* stores stats on what
    subframes were used per-channel */
#endif
};
typedef struct tflac tflac;

extern const char* const tflac_subframe_types[4];

/* runtime CPU features detection, should be called once, globally */
TFLAC_PUBLIC
void tflac_detect_cpu(void);

/* alternative, enable/disable SSE2 globally, returns 0 on success, anything
 * else is an error */
TFLAC_PUBLIC
int tflac_default_sse2(int enable);

TFLAC_PUBLIC
int tflac_default_ssse3(int enable);

TFLAC_PUBLIC
int tflac_default_sse4_1(int enable);

/* you can also enable sse2 on the individual encoder, down below */

/* returns the maximum number of bytes to store a whole FLAC frame */
TFLAC_PUBLIC
TFLAC_CONST
tflac_u32 tflac_size_frame(tflac_u32 blocksize, tflac_u32 channels, tflac_u32 bitdepth);

/* returns the memory size of the tflac struct */
TFLAC_PUBLIC
TFLAC_CONST
tflac_u32 tflac_size(void);

/* returns how much memory is required for storing residuals */
TFLAC_PUBLIC
TFLAC_CONST
tflac_u32 tflac_size_memory(tflac_u32 blocksize);

/* return the size needed to write a STREAMINFO block */
TFLAC_PUBLIC
TFLAC_CONST
tflac_u32 tflac_size_streaminfo(void);

/* initialize a tflac struct */
TFLAC_PUBLIC
void tflac_init(tflac *);

/* validates that the encoder settings are OK, sets up needed memory pointers */
TFLAC_PUBLIC
int tflac_validate(tflac *, void* ptr, tflac_u32 len);

TFLAC_PUBLIC
int tflac_encode_s16p(tflac *, tflac_u32 blocksize, tflac_s16** samples, void* buffer, tflac_u32 len, tflac_u32* used);

TFLAC_PUBLIC
int tflac_encode_s16i(tflac *, tflac_u32 blocksize, tflac_s16* samples, void* buffer, tflac_u32 len, tflac_u32* used);

TFLAC_PUBLIC
int tflac_encode_s32p(tflac *, tflac_u32 blocksize, tflac_s32** samples, void* buffer, tflac_u32 len, tflac_u32* used);

TFLAC_PUBLIC
int tflac_encode_s32i(tflac *, tflac_u32 blocksize, tflac_s32* samples, void* buffer, tflac_u32 len, tflac_u32* used);

/* computes the final MD5 digest, if it was enabled */
TFLAC_PUBLIC
void tflac_finalize(tflac *);

/* encode a STREAMINFO block */
TFLAC_PUBLIC
int tflac_encode_streaminfo(const tflac *, tflac_u32 lastflag, void* buffer, tflac_u32 len, tflac_u32* used);

/* setters for various fields */
TFLAC_PUBLIC
void tflac_set_blocksize(tflac *t, tflac_u32 blocksize);

TFLAC_PUBLIC
void tflac_set_samplerate(tflac *t, tflac_u32 samplerate);

TFLAC_PUBLIC
void tflac_set_channels(tflac *t, tflac_u32 channels);

TFLAC_PUBLIC
void tflac_set_bitdepth(tflac *t, tflac_u32 bitdepth);

TFLAC_PUBLIC
void tflac_set_channel_mode(tflac *t, tflac_u32 channel_mode);

TFLAC_PUBLIC
void tflac_set_max_rice_value(tflac *t, tflac_u32 max_rice_value);

TFLAC_PUBLIC
void tflac_set_min_partition_order(tflac *t, tflac_u32 min_partition_order);

TFLAC_PUBLIC
void tflac_set_max_partition_order(tflac *t, tflac_u32 max_partition_order);

TFLAC_PUBLIC
void tflac_set_constant_subframe(tflac* t, tflac_u32 enable);

TFLAC_PUBLIC
void tflac_set_fixed_subframe(tflac* t, tflac_u32 enable);

TFLAC_PUBLIC
void tflac_set_enable_md5(tflac* t, tflac_u32 enable);

/* one of the few setters that can return an error, try
 * to set the default to use sse2. returns 0 on success,
 * 1 on error (because SSE2 support was not compiled */
TFLAC_PUBLIC
tflac_u32 tflac_enable_sse2(tflac* t, tflac_u32 enable);

TFLAC_PUBLIC
tflac_u32 tflac_enable_ssse3(tflac* t, tflac_u32 enable);

TFLAC_PUBLIC
tflac_u32 tflac_enable_sse4_1(tflac* t, tflac_u32 enable);


/* getters for various fields */
TFLAC_PURE
TFLAC_PUBLIC
tflac_u32 tflac_get_blocksize(const tflac* t);

TFLAC_PURE
TFLAC_PUBLIC
tflac_u32 tflac_get_samplerate(const tflac* t);

TFLAC_PURE
TFLAC_PUBLIC
tflac_u32 tflac_get_channels(const tflac* t);

TFLAC_PURE
TFLAC_PUBLIC
tflac_u32 tflac_get_bitdepth(const tflac* t);

TFLAC_PURE
TFLAC_PUBLIC
tflac_u32 tflac_get_channel_mode(const tflac* t);

TFLAC_PURE
TFLAC_PUBLIC
tflac_u32 tflac_get_max_rice_value(const tflac* t);

TFLAC_PURE
TFLAC_PUBLIC
tflac_u32 tflac_get_min_partition_order(const tflac* t);

TFLAC_PURE
TFLAC_PUBLIC
tflac_u32 tflac_get_max_partition_order(const tflac* t);

TFLAC_PURE
TFLAC_PUBLIC
tflac_u32 tflac_get_constant_subframe(const tflac* t);

TFLAC_PURE
TFLAC_PUBLIC
tflac_u32 tflac_get_fixed_subframe(const tflac* t);

TFLAC_PURE
TFLAC_PUBLIC
tflac_u32 tflac_get_enable_md5(const tflac* t);

TFLAC_PURE
TFLAC_PUBLIC
tflac_u32 tflac_get_enable_sse2(const tflac* t);

TFLAC_PURE
TFLAC_PUBLIC
tflac_u32 tflac_get_enable_ssse3(const tflac* t);

TFLAC_PURE
TFLAC_PUBLIC
tflac_u32 tflac_get_enable_sse4_1(const tflac* t);


#ifdef __cplusplus
}
#endif

#endif /* ifndef TFLAC_HEADER_GUARD */

#ifdef TFLAC_IMPLEMENTATION

#if defined(__x86_64__) || defined(_M_X64)
#define TFLAC_X64
#elif defined(__i386) || defined(_M_IX86)
#define TFLAC_X86
#endif

#ifndef TFLAC_ASSUME_ALIGNED
#if defined(__GNUC__)
#define TFLAC_ASSUME_ALIGNED(x,a) __builtin_assume_aligned(x,a)
#else
#define TFLAC_ASSUME_ALIGNED(x,a) x
#endif
#endif

#ifndef TFLAC_INLINE
#if __STDC_VERSION__ >= 199901L
#define TFLAC_INLINE inline
#elif defined(__GNUC__) && __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)
#define TFLAC_INLINE __inline__
#elif defined(_MSC_VER) && _MSC_VER >= 1200
#define TFLAC_INLINE __inline
#else
#define TFLAC_INLINE
#endif
#endif

#ifndef TFLAC_UNLIKELY
#if defined(__GNUC__) && __GNUC__ >= 3
#define TFLAC_UNLIKELY(x) __builtin_expect(!!(x),0)
#else
#define TFLAC_UNLIKELY(x) (!!(x))
#endif
#endif

#ifndef TFLAC_LIKELY
#if defined(__GNUC__) && __GNUC__ >= 3
#define TFLAC_LIKELY(x) __builtin_expect(!!(x),1)
#else
#define TFLAC_LIKELY(x) (!!(x))
#endif
#endif

#ifndef TFLAC_ASSUME
#if defined(__GNUC__) && __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#define TFLAC_ASSUME(x) do { if (!(x)) __builtin_unreachable(); } while (0);
#elif defined(_MSC_VER) && _MSC_VER >= 1310
#define TFLAC_ASSUME(x) do { if (!(x)) __assume(0); } while(0);
#else
#define TFLAC_ASSUME(x)
#endif
#endif

#ifndef TFLAC_ASSERT
#ifdef NDEBUG
#define TFLAC_ASSERT(x) TFLAC_ASSUME(x)
#else
#include <assert.h>
#define TFLAC_ASSERT(x) assert(x)
#endif
#endif

#ifdef TFLAC_32BIT_ONLY
#define TFLAC_UINT_MAX UINT32_MAX
#define TFLAC_BW_BITS (CHAR_BIT * sizeof(tflac_uint))
#else
#define TFLAC_UINT_MAX UINT64_MAX
#define TFLAC_BW_BITS (CHAR_BIT * sizeof(tflac_uint))
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1400
#include <intrin.h>
#pragma intrinsic(_BitScanForward)
#ifdef TFLAC_X64
#pragma intrinsic(_BitScanForward64)
#endif
#endif

#if defined(TFLAC_X86) || defined(TFLAC_X64)
#ifdef __GNUC__
#include <cpuid.h>
#endif
#endif

#ifndef TFLAC_DISABLE_SSE2
/* TODO check for compiler support */
#ifdef __SSE2__
#define TFLAC_ENABLE_SSE2
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1400
#define TFLAC_ENABLE_SSE2
#if _MSC_VER < 1500
#define _mm_cvtsi128_si64(x) _mm_cvtsi128_si64x(x)
#endif
#endif

#endif


#ifndef TFLAC_DISABLE_SSSE3
#ifdef __SSSE3__
#define TFLAC_ENABLE_SSSE3
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1500
#define TFLAC_ENABLE_SSSE3
#endif
#endif /* DISABLE_SSSE3 */


#ifndef TFLAC_DISABLE_SSE4_1

#ifdef __SSE4_1__
#define TFLAC_ENABLE_SSE4_1
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1500
#define TFLAC_ENABLE_SSE4_1
#endif

#endif

#ifdef TFLAC_ENABLE_SSE2
#include <emmintrin.h>
#endif

#ifdef TFLAC_ENABLE_SSSE3
#include <tmmintrin.h>
#endif

#ifdef TFLAC_ENABLE_SSE4_1
#include <smmintrin.h>
#endif

#ifdef TFLAC_32BIT_ONLY

TFLAC_PRIVATE TFLAC_INLINE
void tflac_u64_cast(tflac_u64* u, tflac_u32 v) {
    u->hi = 0;
    u->lo = v;
}

TFLAC_PRIVATE TFLAC_INLINE
void tflac_u64_add(tflac_u64* a, const tflac_u64* b) {
    a->hi += b->hi;
    a->hi += (a->lo += b->lo) < b->lo;
}

TFLAC_PRIVATE TFLAC_INLINE
void tflac_u64_sub(tflac_u64* a, const tflac_u64* b) {
    tflac_u32 t = a->lo;
    a->hi -= b->hi;
    a->hi -= (a->lo -= b->lo) > t;
}

TFLAC_PRIVATE TFLAC_INLINE
void tflac_u64_add_word(tflac_u64* a, tflac_u32 b) {
    a->hi += (a->lo += b) < b;
}

TFLAC_PRIVATE TFLAC_INLINE
int tflac_u64_cmp(const tflac_u64* a, const tflac_u64* b) {
    if(a->hi == b->hi) {
        if(a->lo == b->lo) return 0;
        return a->lo < b->lo ? -1 : 1;
    }
    return a->hi < b->hi ? -1 : 1;
}

TFLAC_PRIVATE TFLAC_INLINE
int tflac_u64_cmp_word(const tflac_u64* a, tflac_u32 b) {
    if(a->hi) return 1;

    if(a->lo == b) return 0;

    return a->lo < b ? -1 : 1;
}

TFLAC_PRIVATE TFLAC_INLINE
void tflac_s64_cast(tflac_s64* s, tflac_s32 v) {
    s->hi = (tflac_u32)(v >> 31);
    s->lo = (tflac_u32)(v);
}

#define tflac_s64_add(a,b) tflac_u64_add( (tflac_u64*)(a), (const tflac_u64*)(b) )

TFLAC_PRIVATE TFLAC_INLINE
void tflac_s64_abs(tflac_u64* u, const tflac_s64* a) {
    const tflac_s32 mask = ((tflac_s32)a->hi) >> 31;
    tflac_u32 carry;

    carry = (u->lo = (a->lo ^ mask) - mask) < a->lo;
    u->hi = (a->hi ^ mask) - mask - carry;
}

TFLAC_PRIVATE TFLAC_INLINE
void tflac_s64_neg(tflac_s64* s) {
    s->hi = ~s->hi;
    s->lo = ~s->lo;
    s->hi += (s->lo += 1) == 0;
}


TFLAC_PRIVATE TFLAC_INLINE
void tflac_s64_cast32(tflac_s32* d, const tflac_s64* s) {
    *d = s->lo;
}


#define TFLAC_U64_CAST(x,y) (tflac_u64_cast(&(x),(tflac_u32)(y)))
#define TFLAC_U64_ADD_WORD(x,y) (tflac_u64_add_word(&x,(tflac_u32)(y)))

#define TFLAC_U64_ADD(x,y) (tflac_u64_add(&(x),&(y)))

#define TFLAC_U64_GT(x,y) (tflac_u64_cmp(&(x),&(y)) == 1)
#define TFLAC_U64_LT(x,y) (tflac_u64_cmp(&(x),&(y)) == -1)
#define TFLAC_U64_EQ(x,y) (tflac_u64_cmp(&(x),&(y)) == 0)

#define TFLAC_U64_GT_WORD(x,y) (tflac_u64_cmp_word(&(x),y) == 1)
#define TFLAC_U64_LT_WORD(x,y) (tflac_u64_cmp_word(&(x),y) == -1)
#define TFLAC_U64_EQ_WORD(x,y) (tflac_u64_cmp_word(&(x),y) == 0)

#define TFLAC_S64_CAST(x,y) (tflac_s64_cast(&(x),(tflac_s32)(y)))
#define TFLAC_S64_NEG(x) (tflac_s64_neg(&(x)))
#define TFLAC_S64_ADD(x,y) (tflac_s64_add(&(x),&(y)))
#define TFLAC_S64_ABS(x,y) (tflac_s64_abs(&(x),&(y)))
#define TFLAC_S64_CAST32(x,y) (tflac_s64_cast32( &(x), &(y) ))

static const tflac_u64 TFLAC_U64_ZERO = { 0, 0 };
static const tflac_u64 TFLAC_U64_MAX = { UINT32_MAX, UINT32_MAX } ;

#else

TFLAC_PRIVATE TFLAC_INLINE
tflac_s64 tflac_s64_abs(tflac_s64 v) {
#ifdef __GNUC__
    return __builtin_llabs(v);
#else
    const tflac_s64 mask = v >> 63;
    return (v ^ mask) - mask;
#endif
}

#define TFLAC_U64_CAST(x,y) ( (x) = (tflac_u64)(y) )
#define TFLAC_U64_ADD_WORD(x,y) ((x) += (tflac_u64)(y) )

#define TFLAC_U64_ADD(x,y) ( (x) += (y) )

#define TFLAC_U64_GT(x,y) ((x) > (y))
#define TFLAC_U64_LT(x,y) ((x) < (y))
#define TFLAC_U64_EQ(x,y) ((x) == (y))

#define TFLAC_U64_GT_WORD(x,y) ((x) > (tflac_u64)(y))
#define TFLAC_U64_LT_WORD(x,y) ((x) < (tflac_u64)(y))
#define TFLAC_U64_EQ_WORD(x,y) ((x) == (tflac_u64)(y))

#define TFLAC_S64_CAST(x,y) ( (x) = (tflac_s64)(y) )
#define TFLAC_S64_ADD(x,y) ( (x) += (y) )
#define TFLAC_S64_ABS(x,y) ( (x) = (tflac_u64)(tflac_s64_abs((y))) )
#define TFLAC_S64_CAST32(x,y) ( (x) = (tflac_s32)(y) )

#define TFLAC_U64_ZERO UINT64_C(0)
#define TFLAC_U64_MAX UINT64_MAX

#endif

TFLAC_PRIVATE
TFLAC_INLINE tflac_s32 tflac_s32_abs(tflac_s32 v) {
#ifdef __GNUC__
    return __builtin_abs(v);
#else
    const tflac_s32 mask = v >> 31;
    return (v ^ mask) - mask;
#endif
}

TFLAC_PRIVATE
TFLAC_INLINE
tflac_u16 tflac_unpack_u16be(const tflac_u8* d) {
    return (((tflac_u16)d[1])    ) |
           (((tflac_u16)d[0])<< 8);
}

TFLAC_PRIVATE
TFLAC_INLINE
tflac_u32 tflac_unpack_u32le(const tflac_u8* d) {
    return (((tflac_u32)d[0])    ) |
           (((tflac_u32)d[1])<< 8) |
           (((tflac_u32)d[2])<<16) |
           (((tflac_u32)d[3])<<24);
}

TFLAC_PRIVATE
TFLAC_INLINE
tflac_u32 tflac_unpack_u32be(const tflac_u8* d) {
    return (((tflac_u32)d[3])    ) |
           (((tflac_u32)d[2])<< 8) |
           (((tflac_u32)d[1])<<16) |
           (((tflac_u32)d[0])<<24);
}

TFLAC_PRIVATE
TFLAC_INLINE
void tflac_pack_u32le(tflac_u8* d, tflac_u32 n) {
    d[0] = (tflac_u8)( n       );
    d[1] = (tflac_u8)( n >> 8  );
    d[2] = (tflac_u8)( n >> 16 );
    d[3] = (tflac_u8)( n >> 24 );
}

TFLAC_PRIVATE
TFLAC_INLINE
void tflac_pack_u32be(tflac_u8* d, tflac_u32 n) {
    d[0] = (tflac_u8)( n >> 24 );
    d[1] = (tflac_u8)( n >> 16 );
    d[2] = (tflac_u8)( n >>  8 );
    d[3] = (tflac_u8)( n       );
}

TFLAC_PRIVATE
TFLAC_INLINE
void tflac_pack_u64le(tflac_u8* d, tflac_u64 n) {
#ifdef TFLAC_32BIT_ONLY
    tflac_pack_u32le(&d[0],n.lo);
    tflac_pack_u32le(&d[4],n.hi);
#else
    d[0] = (tflac_u8)( n       );
    d[1] = (tflac_u8)( n >> 8  );
    d[2] = (tflac_u8)( n >> 16 );
    d[3] = (tflac_u8)( n >> 24 );
    d[4] = (tflac_u8)( n >> 32 );
    d[5] = (tflac_u8)( n >> 40 );
    d[6] = (tflac_u8)( n >> 48 );
    d[7] = (tflac_u8)( n >> 56 );
#endif
}

TFLAC_PRIVATE
TFLAC_INLINE
void tflac_pack_u64be(tflac_u8* d, tflac_u64 n) {
#ifdef TFLAC_32BIT_ONLY
    tflac_pack_u32be(d,n.hi);
    tflac_pack_u32be(d,n.lo);
#else
    d[0] = (tflac_u8)( n >> 56 );
    d[1] = (tflac_u8)( n >> 48 );
    d[2] = (tflac_u8)( n >> 40 );
    d[3] = (tflac_u8)( n >> 32 );
    d[4] = (tflac_u8)( n >> 24 );
    d[5] = (tflac_u8)( n >> 16 );
    d[6] = (tflac_u8)( n >>  8 );
    d[7] = (tflac_u8)( n       );
#endif
}

typedef void (*tflac_md5_calculator)(tflac*, void* samples);
typedef void (*tflac_stereo_decorrelator)(tflac*, tflac_u32 channel, void* samples);

struct tflac_encode_params {
    tflac_u32 blocksize;
    tflac_u32 buffer_len;
    void* buffer;
    void* samples;
    tflac_u32 *used;
    tflac_md5_calculator calculate_md5;
    tflac_stereo_decorrelator decorrelate;
};
typedef struct tflac_encode_params tflac_encode_params;

const char* const tflac_subframe_types[4] = {
    "CONSTANT",
    "VERBATIM",
    "FIXED",
    "LPC",
};

TFLAC_PRIVATE
TFLAC_CONST
tflac_u32 tflac_max_size_frame(tflac_u32 blocksize, tflac_u32 channels, tflac_u32 bitdepth) {
    /* since blocksize is really a 16-bit value and everything else is way under that
     * we don't have to worry about overflow */

    /* the max bytes used for frame headers and footer is 18:
     *   2 for frame sync + blocking strategy
     *   1 for block size + sample rate
     *   1 for channel assignment and sample size
     *   7 for maximum sample number (we use frame number so it's really 6)
     *   2 for optional 16-bit blocksize
     *   2 for optional 16-bit samplerate
     *   1 for crc8
     *   2 for crc16 */

    /* additionally if the bitdepth is less than 32 we could have a bitdepth+1 subframe */
    return UINT32_C(18) +
      /* each subframe header takes 1 byte */
      channels +
      ((
        /* for non-stereo files we always use independent subframes */
        (blocksize * bitdepth * (channels * (channels != 2))) +
        /* on a stereo file we will have 1 subframe at our current bitdepth */
        (blocksize * bitdepth * (channels == 2)) +
        /* and we may have 1 subframe at bitdepth + 1, unless it's 32 */
        (blocksize * (bitdepth + (bitdepth != 32)) * (channels == 2)) +
        /* and we add 7 bits in case we're not 8-bit aligned */
        + 7
      ) / 8);
}

TFLAC_PUBLIC
TFLAC_CONST
tflac_u32 tflac_size_frame(tflac_u32 blocksize, tflac_u32 channels, tflac_u32 bitdepth) {
    /* pad the max frame size with an extra 8 bytes for bitwriter to overflow into */
    return 8 + tflac_max_size_frame(blocksize, channels, bitdepth);
}

TFLAC_PUBLIC
TFLAC_CONST
tflac_u32 tflac_size_memory(tflac_u32 blocksize) {
    /* assuming we need everything on a 16-byte alignment */
    return
      (tflac_u32) UINT32_C(15) + (UINT32_C(5) * ( (UINT32_C(15) + (blocksize * UINT32_C(4))) & UINT32_C(0xFFFFFFF0)));
}

TFLAC_PRIVATE
TFLAC_INLINE
void tflac_bitwriter_init(tflac_bitwriter*);

TFLAC_PRIVATE
TFLAC_INLINE
int tflac_bitwriter_flush(tflac_bitwriter*);

TFLAC_PRIVATE
TFLAC_INLINE
int tflac_bitwriter_align(tflac_bitwriter*);

TFLAC_PRIVATE
TFLAC_INLINE
int tflac_bitwriter_zeroes(tflac_bitwriter*, tflac_u32 bits);

TFLAC_PRIVATE
TFLAC_INLINE
int tflac_bitwriter_add(tflac_bitwriter*, tflac_u32 bits, tflac_uint val);

TFLAC_PRIVATE
TFLAC_INLINE
void tflac_md5_init(tflac_md5* m);

TFLAC_PRIVATE
TFLAC_INLINE
void tflac_md5_transform(tflac_md5* m);

TFLAC_PRIVATE
TFLAC_INLINE
void tflac_md5_addsample(tflac_md5* m, tflac_u32 bits, tflac_uint val);

TFLAC_PRIVATE
TFLAC_INLINE
void tflac_md5_finalize(tflac_md5* m);

TFLAC_PRIVATE int tflac_encode(tflac* t, const tflac_encode_params* p);
TFLAC_PRIVATE int tflac_encode_frame_header(tflac *);

TFLAC_CONST TFLAC_PRIVATE TFLAC_INLINE
tflac_u32 tflac_wasted_bits(tflac_s32 sample, tflac_u32 bits);

TFLAC_PRIVATE void tflac_stereo_decorrelate_independent_int16(tflac*, tflac_u32 channel, tflac_u32 stride, const tflac_s16* samples, void*);
TFLAC_PRIVATE void tflac_stereo_decorrelate_independent_int32(tflac*, tflac_u32 channel, tflac_u32 stride, const tflac_s32* samples, void*);

TFLAC_PRIVATE void tflac_stereo_decorrelate_left_side_int16(tflac*, tflac_u32 channel, tflac_u32 stride, const tflac_s16* left, const tflac_s16* right);
TFLAC_PRIVATE void tflac_stereo_decorrelate_left_side_int32(tflac*, tflac_u32 channel, tflac_u32 stride, const tflac_s32* left, const tflac_s32* right);

TFLAC_PRIVATE void tflac_stereo_decorrelate_side_right_int16(tflac*, tflac_u32 channel, tflac_u32 stride, const tflac_s16* left, const tflac_s16* right);
TFLAC_PRIVATE void tflac_stereo_decorrelate_side_right_int32(tflac*, tflac_u32 channel, tflac_u32 stride, const tflac_s32* left, const tflac_s32* right);

TFLAC_PRIVATE void tflac_stereo_decorrelate_mid_side_int16(tflac*, tflac_u32 channel, tflac_u32 stride, const tflac_s16* left, const tflac_s16* right);
TFLAC_PRIVATE void tflac_stereo_decorrelate_mid_side_int32(tflac*, tflac_u32 channel, tflac_u32 stride, const tflac_s32* left, const tflac_s32* right);

TFLAC_PRIVATE void tflac_stereo_decorrelate_int16_planar(tflac*, tflac_u32 channel, const tflac_s16** samples);
TFLAC_PRIVATE void tflac_stereo_decorrelate_int16_interleaved(tflac*, tflac_u32 channel, const tflac_s16* samples);

TFLAC_PRIVATE void tflac_stereo_decorrelate_int32_planar(tflac*, tflac_u32 channel, const tflac_s32** samples);
TFLAC_PRIVATE void tflac_stereo_decorrelate_int32_interleaved(tflac*, tflac_u32 channel, const tflac_s32* samples);


TFLAC_PRIVATE void (*tflac_cfr_order0)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void (*tflac_cfr_order1)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void (*tflac_cfr_order2)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void (*tflac_cfr_order3)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void (*tflac_cfr_order4)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);


TFLAC_PRIVATE void (*tflac_cfr_order1_wide)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void (*tflac_cfr_order2_wide)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void (*tflac_cfr_order3_wide)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void (*tflac_cfr_order4_wide)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);

TFLAC_PRIVATE void tflac_cfr_order0_std(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void tflac_cfr_order1_std(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void tflac_cfr_order2_std(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void tflac_cfr_order3_std(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void tflac_cfr_order4_std(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);

#ifdef TFLAC_ENABLE_SSE2
TFLAC_PRIVATE void tflac_cfr_order0_sse2(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void tflac_cfr_order1_sse2(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void tflac_cfr_order2_sse2(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void tflac_cfr_order3_sse2(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void tflac_cfr_order4_sse2(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
#endif

#ifdef TFLAC_ENABLE_SSSE3
TFLAC_PRIVATE void tflac_cfr_order0_ssse3(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void tflac_cfr_order1_ssse3(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void tflac_cfr_order2_ssse3(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void tflac_cfr_order3_ssse3(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void tflac_cfr_order4_ssse3(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
#endif

#ifdef TFLAC_ENABLE_SSE4_1
TFLAC_PRIVATE void tflac_cfr_order0_sse4_1(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);

TFLAC_PRIVATE void tflac_cfr_order1_sse4_1(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);

TFLAC_PRIVATE void tflac_cfr_order2_sse4_1(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);

TFLAC_PRIVATE void tflac_cfr_order3_sse4_1(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void tflac_cfr_order4_sse4_1(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
#endif

/* variant functions that convert samples to 64-bit then calculates,
 * used when bps >= 32, 31, 30, 29 */

/* note, there is no order0 wide_std, we just default to
 * the default order0 function since checking for
 * INT32_MIN was already handled in sample analysis */

TFLAC_PRIVATE void tflac_cfr_order1_wide_std(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void tflac_cfr_order2_wide_std(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void tflac_cfr_order3_wide_std(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);
TFLAC_PRIVATE void tflac_cfr_order4_wide_std(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT samples,
    tflac_s32* TFLAC_RESTRICT residuals,
    tflac_u64* TFLAC_RESTRICT residual_error
);

TFLAC_PRIVATE void tflac_cfr(tflac*);

/* encodes a constant subframe iff value is constant, this should always be tried first */
TFLAC_PRIVATE int tflac_encode_subframe_constant(tflac*);

/* encodes a fixed subframe only if the length < verbatim */
TFLAC_PRIVATE int tflac_encode_subframe_fixed(tflac*);

/* encodes a subframe verbatim, only fails if the buffer runs out of room */
TFLAC_PRIVATE int tflac_encode_subframe_verbatim(tflac*);

/* encodes a subframe, tries constant, fixed, then verbatim */
TFLAC_PRIVATE int tflac_encode_subframe(tflac*, tflac_u8 channel);

TFLAC_PRIVATE int tflac_encode_residuals(tflac*, tflac_u8 predictor_order, tflac_u8 partition_order);

/* various tables to define at the end of the file */
TFLAC_PRIVATE const tflac_u16 tflac_crc16_tables[8][256];
TFLAC_PRIVATE const tflac_u8 tflac_crc8_table[256];


TFLAC_PRIVATE TFLAC_INLINE tflac_u8 tflac_crc8(const tflac_u8* d, tflac_u32 len, tflac_u8 crc8) {
    tflac_u32 i = 0;
    for(i=0;i<len;i++) {
        crc8 = tflac_crc8_table[crc8 ^ d[i]];
    }
    return crc8;
}

/*
 * https://freac.org/developer-blog-mainmenu-9/14-freac/277-fastcrc
 */
TFLAC_PRIVATE TFLAC_INLINE tflac_u16 tflac_crc16(const tflac_u8* d, tflac_u32 len, tflac_u16 crc16) {
    while(len >= 8) {
        crc16 ^= d[0] << 8 | d[1];
        crc16 =
          tflac_crc16_tables[7][crc16 >> 8] ^ tflac_crc16_tables[6][crc16 & 0xFF] ^
          tflac_crc16_tables[5][d[2]]       ^ tflac_crc16_tables[4][d[3]] ^
          tflac_crc16_tables[3][d[4]]       ^ tflac_crc16_tables[2][d[5]] ^
          tflac_crc16_tables[1][d[6]]       ^ tflac_crc16_tables[0][d[7]];
          d   += 8;
          len -= 8;
    }

    while(len--) {
        crc16 = (crc16 << 8) ^ tflac_crc16_tables[0][(crc16 >> 8) ^ *d++];
    }

    return crc16;
}

TFLAC_CONST TFLAC_PRIVATE TFLAC_INLINE
tflac_u32 tflac_wasted_bits(tflac_s32 sample, tflac_u32 bits) {
#if defined(_MSC_VER) && _MSC_VER >= 1400
#ifdef TFLAC_X64
    unsigned long index;
    _BitScanForward64(&index, (UINT64_C(0xFFFFFFFFFFFFFFFF) << bits) | (unsigned __int64)sample);
    return (tflac_u32)index;
#else
    unsigned long index;
    if(sample == 0) return bits;
    _BitScanForward(&index, (unsigned long) sample);
    return (tflac_u32)index;
#endif
#elif __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
    return (tflac_u32)__builtin_ctzll( (UINT64_C(0xFFFFFFFFFFFFFFFF) << bits) | (unsigned long long)sample);
#else
    /* TODO optimize for other platforms? */
    tflac_u32 s = (tflac_u32)sample;
    tflac_u32 i = 0;
    --bits;
    while(i < bits) {
        if(s >> i & 1) break;
        i++;
    }
    return i;
#endif
}

#define TFLAC_MD5_LEFTROTATE(x, s) (x << s | x >> (32-s))

TFLAC_PRIVATE
TFLAC_INLINE
void tflac_bitwriter_init(tflac_bitwriter* bw) {
    bw->val = 0;
    bw->bits = 0;
    bw->pos = 0;
    bw->len = 0;
    bw->tot = 0;
    bw->buffer = NULL;
}

TFLAC_PRIVATE
TFLAC_INLINE
int tflac_bitwriter_flush(tflac_bitwriter* bw) {
    tflac_u32 bytes = 0;
    tflac_u32 bits = 0;

    TFLAC_ASSERT(bw->bits != TFLAC_BW_BITS);

    bytes = bw->bits / CHAR_BIT;
    bits  = bw->bits % CHAR_BIT;

    if(bytes > bw->len - bw->pos) return -1;

    tflac_pack_uintbe(&bw->buffer[bw->pos],bw->val);

    bw->pos += bytes;
    bw->bits = bits;

    bw->val = ((tflac_uint)(bw->buffer[bw->pos])) << ( TFLAC_BW_BITS - CHAR_BIT );

    return 0;
}


/* used to write a bunch of zero-bits */
TFLAC_PRIVATE
TFLAC_INLINE
int tflac_bitwriter_zeroes(tflac_bitwriter* bw, tflac_u32 bits) {
    tflac_u32 bytes = 0;

    bw->tot += bits;

    bits += bw->bits;

    bytes = bits / CHAR_BIT;
    bits = bits % CHAR_BIT;

    if(bytes > bw->len - bw->pos) return -1;

    tflac_pack_uintbe(&bw->buffer[bw->pos],bw->val);

    while(bytes >= sizeof(tflac_uint)) {
        bw->pos += (tflac_u32)sizeof(tflac_uint);
        bytes   -= (tflac_u32)sizeof(tflac_uint);
        tflac_pack_uintbe(&bw->buffer[bw->pos],0);
    }

    bw->pos += bytes;
    bw->bits = bits;

    bw->val = ((tflac_uint)(bw->buffer[bw->pos])) << ( TFLAC_BW_BITS - CHAR_BIT );

    return 0;
}

TFLAC_PRIVATE
TFLAC_INLINE
int tflac_bitwriter_add(tflac_bitwriter *bw, tflac_u32 bits, tflac_uint val) {
    const tflac_uint mask = TFLAC_UINT_MAX << 1;
    tflac_u32 b;
    int r;

    TFLAC_ASSERT(bits > 0);

    val <<= (TFLAC_BW_BITS - bits);
    bw->tot += bits;

    while(bw->bits + bits >= TFLAC_BW_BITS) {
        b = TFLAC_BW_BITS - bw->bits - 1;
        b = b > bits ? bits : b;
        bw->val |= (val >> bw->bits);
        bw->bits += b;
        bw->val &= mask; /* always make sure the last bit is 0 */
        if( (r = tflac_bitwriter_flush(bw)) != 0) return r;
        val <<= b;
        bits -= b;
    }

    TFLAC_ASSERT(bw->bits + bits < TFLAC_BW_BITS);

    bw->val |= (val >> bw->bits);
    bw->bits += bits;

    return 0;
}


TFLAC_PRIVATE
TFLAC_INLINE
int tflac_bitwriter_align(tflac_bitwriter *bw) {
    tflac_u32 rem =  (CHAR_BIT - (bw->bits % 8)) % CHAR_BIT;
    if(rem) return tflac_bitwriter_add(bw, rem, 0);
    return 0;
}

TFLAC_PRIVATE
TFLAC_INLINE
void tflac_md5_init(tflac_md5* m) {
    m->a = 0x67452301;
    m->b = 0xefcdab89;
    m->c = 0x98badcfe;
    m->d = 0x10325476;
    m->total = TFLAC_U64_ZERO;
    m->pos = 0;
}

TFLAC_PRIVATE
TFLAC_INLINE
void tflac_md5_transform(tflac_md5* m) {
    tflac_u32 A = m->a;
    tflac_u32 B = m->b;
    tflac_u32 C = m->c;
    tflac_u32 D = m->d;

    tflac_u32 F = 0;

    tflac_u32 M[16];

    M[0]  = tflac_unpack_u32le(&m->buffer[  (0  * 4) ]);
    M[1]  = tflac_unpack_u32le(&m->buffer[  (1  * 4) ]);
    M[2]  = tflac_unpack_u32le(&m->buffer[  (2  * 4) ]);
    M[3]  = tflac_unpack_u32le(&m->buffer[  (3  * 4) ]);
    M[4]  = tflac_unpack_u32le(&m->buffer[  (4  * 4) ]);
    M[5]  = tflac_unpack_u32le(&m->buffer[  (5  * 4) ]);
    M[6]  = tflac_unpack_u32le(&m->buffer[  (6  * 4) ]);
    M[7]  = tflac_unpack_u32le(&m->buffer[  (7  * 4) ]);
    M[8]  = tflac_unpack_u32le(&m->buffer[  (8  * 4) ]);
    M[9]  = tflac_unpack_u32le(&m->buffer[  (9  * 4) ]);
    M[10] = tflac_unpack_u32le(&m->buffer[ (10  * 4) ]);
    M[11] = tflac_unpack_u32le(&m->buffer[ (11  * 4) ]);
    M[12] = tflac_unpack_u32le(&m->buffer[ (12  * 4) ]);
    M[13] = tflac_unpack_u32le(&m->buffer[ (13  * 4) ]);
    M[14] = tflac_unpack_u32le(&m->buffer[ (14  * 4) ]);
    M[15] = tflac_unpack_u32le(&m->buffer[ (15  * 4) ]);


#define TFLAC_MD5_TRANSFORM_TAIL(g, s, k, expr) \
        F = A + k + M[g] + (expr); \
        A = D; \
        D = C; \
        C = B; \
        B += TFLAC_MD5_LEFTROTATE(F, s);

#define TFLAC_MD5_ROUND1(g,s,k) \
        TFLAC_MD5_TRANSFORM_TAIL(g, s, k, D ^ (B & (C ^ D)))

#define TFLAC_MD5_ROUND2(g,s,k) \
        TFLAC_MD5_TRANSFORM_TAIL(g, s, k, C ^ (D & (B ^ C)))

#define TFLAC_MD5_ROUND3(g,s,k) \
        TFLAC_MD5_TRANSFORM_TAIL(g, s, k, B ^ C ^ D)

#define TFLAC_MD5_ROUND4(g,s,k) \
        TFLAC_MD5_TRANSFORM_TAIL(g, s, k, C ^ (B | (~D) ) )

    TFLAC_MD5_ROUND1( 0,  7, UINT32_C(0x0d76aa478))
    TFLAC_MD5_ROUND1( 1, 12, UINT32_C(0x0e8c7b756))
    TFLAC_MD5_ROUND1( 2, 17, UINT32_C(0x0242070db))
    TFLAC_MD5_ROUND1( 3, 22, UINT32_C(0x0c1bdceee))
    TFLAC_MD5_ROUND1( 4,  7, UINT32_C(0x0f57c0faf))
    TFLAC_MD5_ROUND1( 5, 12, UINT32_C(0x04787c62a))
    TFLAC_MD5_ROUND1( 6, 17, UINT32_C(0x0a8304613))
    TFLAC_MD5_ROUND1( 7, 22, UINT32_C(0x0fd469501))
    TFLAC_MD5_ROUND1( 8,  7, UINT32_C(0x0698098d8))
    TFLAC_MD5_ROUND1( 9, 12, UINT32_C(0x08b44f7af))
    TFLAC_MD5_ROUND1(10, 17, UINT32_C(0x0ffff5bb1))
    TFLAC_MD5_ROUND1(11, 22, UINT32_C(0x0895cd7be))
    TFLAC_MD5_ROUND1(12,  7, UINT32_C(0x06b901122))
    TFLAC_MD5_ROUND1(13, 12, UINT32_C(0x0fd987193))
    TFLAC_MD5_ROUND1(14, 17, UINT32_C(0x0a679438e))
    TFLAC_MD5_ROUND1(15, 22, UINT32_C(0x049b40821))

    TFLAC_MD5_ROUND2( 1,  5, UINT32_C(0x0f61e2562))
    TFLAC_MD5_ROUND2( 6,  9, UINT32_C(0x0c040b340))
    TFLAC_MD5_ROUND2(11, 14, UINT32_C(0x0265e5a51))
    TFLAC_MD5_ROUND2( 0, 20, UINT32_C(0x0e9b6c7aa))
    TFLAC_MD5_ROUND2( 5,  5, UINT32_C(0x0d62f105d))
    TFLAC_MD5_ROUND2(10,  9, UINT32_C(0x002441453))
    TFLAC_MD5_ROUND2(15, 14, UINT32_C(0x0d8a1e681))
    TFLAC_MD5_ROUND2( 4, 20, UINT32_C(0x0e7d3fbc8))
    TFLAC_MD5_ROUND2( 9,  5, UINT32_C(0x021e1cde6))
    TFLAC_MD5_ROUND2(14,  9, UINT32_C(0x0c33707d6))
    TFLAC_MD5_ROUND2( 3, 14, UINT32_C(0x0f4d50d87))
    TFLAC_MD5_ROUND2( 8, 20, UINT32_C(0x0455a14ed))
    TFLAC_MD5_ROUND2(13,  5, UINT32_C(0x0a9e3e905))
    TFLAC_MD5_ROUND2( 2,  9, UINT32_C(0x0fcefa3f8))
    TFLAC_MD5_ROUND2( 7, 14, UINT32_C(0x0676f02d9))
    TFLAC_MD5_ROUND2(12, 20, UINT32_C(0x08d2a4c8a))

    TFLAC_MD5_ROUND3( 5,  4, UINT32_C(0x0fffa3942))
    TFLAC_MD5_ROUND3( 8, 11, UINT32_C(0x08771f681))
    TFLAC_MD5_ROUND3(11, 16, UINT32_C(0x06d9d6122))
    TFLAC_MD5_ROUND3(14, 23, UINT32_C(0x0fde5380c))
    TFLAC_MD5_ROUND3( 1,  4, UINT32_C(0x0a4beea44))
    TFLAC_MD5_ROUND3( 4, 11, UINT32_C(0x04bdecfa9))
    TFLAC_MD5_ROUND3( 7, 16, UINT32_C(0x0f6bb4b60))
    TFLAC_MD5_ROUND3(10, 23, UINT32_C(0x0bebfbc70))
    TFLAC_MD5_ROUND3(13,  4, UINT32_C(0x0289b7ec6))
    TFLAC_MD5_ROUND3( 0, 11, UINT32_C(0x0eaa127fa))
    TFLAC_MD5_ROUND3( 3, 16, UINT32_C(0x0d4ef3085))
    TFLAC_MD5_ROUND3( 6, 23, UINT32_C(0x004881d05))
    TFLAC_MD5_ROUND3( 9,  4, UINT32_C(0x0d9d4d039))
    TFLAC_MD5_ROUND3(12, 11, UINT32_C(0x0e6db99e5))
    TFLAC_MD5_ROUND3(15, 16, UINT32_C(0x01fa27cf8))
    TFLAC_MD5_ROUND3( 2, 23, UINT32_C(0x0c4ac5665))

    TFLAC_MD5_ROUND4( 0,  6, UINT32_C(0x0f4292244))
    TFLAC_MD5_ROUND4( 7, 10, UINT32_C(0x0432aff97))
    TFLAC_MD5_ROUND4(14, 15, UINT32_C(0x0ab9423a7))
    TFLAC_MD5_ROUND4( 5, 21, UINT32_C(0x0fc93a039))
    TFLAC_MD5_ROUND4(12,  6, UINT32_C(0x0655b59c3))
    TFLAC_MD5_ROUND4( 3, 10, UINT32_C(0x08f0ccc92))
    TFLAC_MD5_ROUND4(10, 15, UINT32_C(0x0ffeff47d))
    TFLAC_MD5_ROUND4( 1, 21, UINT32_C(0x085845dd1))
    TFLAC_MD5_ROUND4( 8,  6, UINT32_C(0x06fa87e4f))
    TFLAC_MD5_ROUND4(15, 10, UINT32_C(0x0fe2ce6e0))
    TFLAC_MD5_ROUND4( 6, 15, UINT32_C(0x0a3014314))
    TFLAC_MD5_ROUND4(13, 21, UINT32_C(0x04e0811a1))
    TFLAC_MD5_ROUND4( 4,  6, UINT32_C(0x0f7537e82))
    TFLAC_MD5_ROUND4(11, 10, UINT32_C(0x0bd3af235))
    TFLAC_MD5_ROUND4( 2, 15, UINT32_C(0x02ad7d2bb))
    TFLAC_MD5_ROUND4( 9, 21, UINT32_C(0x0eb86d391))


    m->a += A;
    m->b += B;
    m->c += C;
    m->d += D;
}

TFLAC_PRIVATE
TFLAC_INLINE
void tflac_md5_addsample(tflac_md5* m, tflac_u32 bits, tflac_uint val) {
    tflac_u32 bytes;

    TFLAC_U64_ADD_WORD(m->total,bits);

    bytes = bits / 8;

    tflac_pack_uintle(&m->buffer[m->pos], val);
    m->pos += bytes;

    if(m->pos >= 64) {
        tflac_md5_transform(m);
        m->pos %= 64;
        bytes = m->pos;
        while(bytes--) {
            m->buffer[bytes] = m->buffer[64+bytes];
        }
    }
}

TFLAC_PRIVATE
TFLAC_INLINE
void tflac_md5_finalize(tflac_md5* m) {
    tflac_u64 len = m->total;
    /* add the stop bit */
    tflac_md5_addsample(m, 8, 0x80);

    /* pad zeroes until we have 8 bytes left */
    while(m->pos != 56) {
        tflac_md5_addsample(m, 8, 0x00);
    }

    tflac_pack_u64le(&m->buffer[56],len);
    tflac_md5_transform(m);
}

TFLAC_PRIVATE
void tflac_md5_digest(const tflac_md5* m, tflac_u8 out[16]) {
    out[0]  = (tflac_u8)(m->a);
    out[1]  = (tflac_u8)(m->a >> 8);
    out[2]  = (tflac_u8)(m->a >> 16);
    out[3]  = (tflac_u8)(m->a >> 24);
    out[4]  = (tflac_u8)(m->b);
    out[5]  = (tflac_u8)(m->b >> 8);
    out[6]  = (tflac_u8)(m->b >> 16);
    out[7]  = (tflac_u8)(m->b >> 24);
    out[8]  = (tflac_u8)(m->c);
    out[9]  = (tflac_u8)(m->c >> 8);
    out[10] = (tflac_u8)(m->c >> 16);
    out[11] = (tflac_u8)(m->c >> 24);
    out[12] = (tflac_u8)(m->d);
    out[13] = (tflac_u8)(m->d >> 8);
    out[14] = (tflac_u8)(m->d >> 16);
    out[15] = (tflac_u8)(m->d >> 24);
}

TFLAC_CONST TFLAC_PRIVATE
tflac_u32 tflac_verbatim_subframe_bits(tflac_u32 blocksize, tflac_u32 bitdepth) {
    return 8 + (blocksize * bitdepth);
}

TFLAC_PRIVATE
int tflac_encode_subframe_verbatim(tflac* t) {
    tflac_u32 i = 0;
    tflac_u8 w = (tflac_u8)t->wasted_bits;
    const tflac_s32* residuals_0 = TFLAC_ASSUME_ALIGNED(t->residuals[0], 16);
    int r;

    if( (r = tflac_bitwriter_add(&t->bw, 8, 0x02 | (!!w) )) != 0) return r;
    if(w) if( (r = tflac_bitwriter_add(&t->bw, w, 1)) != 0) return r;

    for(i=0;i<t->cur_blocksize;i++) {
        if( (r = tflac_bitwriter_add(&t->bw, t->subframe_bitdepth - t->wasted_bits, (tflac_u32)residuals_0[i])) != 0) return r;
    }

    return tflac_bitwriter_flush(&t->bw);
}

TFLAC_PRIVATE
int tflac_encode_subframe_constant(tflac* t) {
    int r;
    if( (r = tflac_bitwriter_add(&t->bw, 8, 0x00)) != 0) return r;
    if( (r = tflac_bitwriter_add(&t->bw, t->subframe_bitdepth, ((tflac_u32) t->residuals[0][0]) << t->wasted_bits)) != 0) return r;
    return tflac_bitwriter_flush(&t->bw);
}

TFLAC_PRIVATE TFLAC_INLINE void tflac_rescale_samples(tflac* t) {
    tflac_u32 i = 0;
    tflac_s32* TFLAC_RESTRICT residuals_0 = TFLAC_ASSUME_ALIGNED(t->residuals[0], 16);

    if( (!t->constant) && t->wasted_bits) {
        /* rescale residuals for order 0 */
        for(i=0;i<t->cur_blocksize;i++) {
            residuals_0[i] = residuals_0[i] / (1 << t->wasted_bits);
        }
        /* since we scaled down there's no way we have INT32_MIN in there anymore */
        t->residual_errors[0] = TFLAC_U64_ZERO;
    }

    return;
}

/* versions of the md5_interleaved that pack samples before calling addsample */

/* used with 1-byte input */
TFLAC_PRIVATE void tflac_update_md5_s16i_1(tflac* t, const tflac_s16* samples) {
    tflac_u32 b = t->cur_blocksize * t->channels;
    const tflac_u32 step = sizeof(tflac_uint);
    tflac_uint v;

    while(b >= step) {
        v  = (((tflac_uint)samples[0]) & 0xFF) << 0;
        v |= (((tflac_uint)samples[1]) & 0xFF) << 8;
        v |= (((tflac_uint)samples[2]) & 0xFF) << 16;
        v |= (((tflac_uint)samples[3]) & 0xFF) << 24;
#ifndef TFLAC_32BIT_ONLY
        v |= (((tflac_uint)samples[4]) & 0xFF) << 32;
        v |= (((tflac_uint)samples[5]) & 0xFF) << 40;
        v |= (((tflac_uint)samples[6]) & 0xFF) << 48;
        v |= (((tflac_uint)samples[7]) & 0xFF) << 56;
#endif
        tflac_md5_addsample(&t->md5_ctx, TFLAC_BW_BITS, v);

        b -= step;
        samples += step;
    }

    while(b--) {
        tflac_md5_addsample(&t->md5_ctx, 8, (tflac_uint)*samples++);
    }
}

TFLAC_PRIVATE void tflac_update_md5_s16i_2(tflac* t, const tflac_s16* samples) {
    tflac_u32 b = t->cur_blocksize * t->channels;
    const tflac_u32 step = (sizeof(tflac_uint)/2);
    tflac_uint v;

    while(b >= step) {
        v  = (((tflac_uint)samples[0]) & UINT32_C(0xFFFF)) << 0;
        v |= (((tflac_uint)samples[1]) & UINT32_C(0xFFFF)) << 16;
#ifndef TFLAC_32BIT_ONLY
        v |= (((tflac_uint)samples[2]) & UINT32_C(0xFFFF)) << 32;
        v |= (((tflac_uint)samples[3]) & UINT32_C(0xFFFF)) << 48;
#endif
        tflac_md5_addsample(&t->md5_ctx, TFLAC_BW_BITS, v);

        b -= step;
        samples += step;
    }

    while(b--) {
        tflac_md5_addsample(&t->md5_ctx, 16, (tflac_uint)*samples++);
    }
}

TFLAC_PRIVATE void tflac_update_md5_s32i_1(tflac* t, const tflac_s32* samples) {
    tflac_u32 b = t->cur_blocksize * t->channels;
    const tflac_u32 step = sizeof(tflac_uint);
    tflac_uint v;

    while(b >= step) {
        v  = (((tflac_uint)samples[0]) & 0xFF) << 0;
        v |= (((tflac_uint)samples[1]) & 0xFF) << 8;
        v |= (((tflac_uint)samples[2]) & 0xFF) << 16;
        v |= (((tflac_uint)samples[3]) & 0xFF) << 24;
#ifndef TFLAC_32BIT_ONLY
        v |= (((tflac_uint)samples[4]) & 0xFF) << 32;
        v |= (((tflac_uint)samples[5]) & 0xFF) << 40;
        v |= (((tflac_uint)samples[6]) & 0xFF) << 48;
        v |= (((tflac_uint)samples[7]) & 0xFF) << 56;
#endif
        tflac_md5_addsample(&t->md5_ctx, TFLAC_BW_BITS, v);

        b -= step;
        samples += step;
    }

    while(b--) {
        tflac_md5_addsample(&t->md5_ctx, 8, (tflac_uint)*samples++);
    }
}

TFLAC_PRIVATE void tflac_update_md5_s32i_2(tflac* t, const tflac_s32* samples) {
    tflac_u32 b = t->cur_blocksize * t->channels;
    const tflac_u32 step = (sizeof(tflac_uint)/2);
    tflac_uint v;

    while(b >= step) {
        v  = (((tflac_uint)samples[0]) & UINT32_C(0xFFFF)) << 0;
        v |= (((tflac_uint)samples[1]) & UINT32_C(0xFFFF)) << 16;
#ifndef TFLAC_32BIT_ONLY
        v |= (((tflac_uint)samples[2]) & UINT32_C(0xFFFF)) << 32;
        v |= (((tflac_uint)samples[3]) & UINT32_C(0xFFFF)) << 48;
#endif
        tflac_md5_addsample(&t->md5_ctx, TFLAC_BW_BITS, v);

        b -= step;
        samples += step;
    }

    while(b--) {
        tflac_md5_addsample(&t->md5_ctx, 16, (tflac_uint)*samples++);
    }
}

TFLAC_PRIVATE void tflac_update_md5_s32i_3(tflac* t, const tflac_s32* samples) {
    tflac_u32 b = t->cur_blocksize * t->channels;

#ifndef TFLAC_32BIT_ONLY
    tflac_uint v;

    while(b >= 2) {
        v  = (((tflac_uint)samples[0]) & UINT32_C(0xFFFFFF)) << 0;
        v |= (((tflac_uint)samples[1]) & UINT32_C(0xFFFFFF)) << 24;
        tflac_md5_addsample(&t->md5_ctx, 48, v);

        b -= 2;
        samples += 2;
    }
#endif

    while(b--) {
        tflac_md5_addsample(&t->md5_ctx, 24,  (tflac_uint) *samples++);
    }
}

TFLAC_PRIVATE void tflac_update_md5_s32i_4(tflac* t, const tflac_s32* samples) {
    tflac_u32 b = t->cur_blocksize * t->channels;

#ifndef TFLAC_32BIT_ONLY
    tflac_uint v;

    while(b >= 2) {
        v  = (((tflac_uint)samples[0]) & UINT32_C(0xFFFFFFFF)) << 0;
        v |= (((tflac_uint)samples[1]) & UINT32_C(0xFFFFFFFF)) << 32;
        tflac_md5_addsample(&t->md5_ctx, 64, v);

        b -= 2;
        samples += 2;
    }
#endif

    while(b--) {
        tflac_md5_addsample(&t->md5_ctx, 32, (tflac_uint) *samples++);
    }
}

TFLAC_PRIVATE void tflac_update_md5_int16_planar(tflac* t, const tflac_s16** samples) {
    tflac_u32 i = 0;
    tflac_u32 c = 0;
    tflac_u32 bits = (7 + t->bitdepth) & 0xF8;

    for(i=0;i<t->cur_blocksize;i++) {
        for(c=0;i<t->channels;c++) {
            tflac_md5_addsample(&t->md5_ctx,bits,(tflac_uint)samples[c][i]);
        }
    }
}

TFLAC_PRIVATE void tflac_update_md5_int32_planar(tflac* t, const tflac_s32** samples) {
    tflac_u32 i = 0;
    tflac_u32 c = 0;
    tflac_u32 bits = (7 + t->bitdepth) & 0xF8;

    for(i=0;i<t->cur_blocksize;i++) {
        for(c=0;i<t->channels;c++) {
            tflac_md5_addsample(&t->md5_ctx,bits,(tflac_uint)samples[c][i]);
        }
    }
}


TFLAC_PRIVATE void tflac_stereo_decorrelate_independent_int16(tflac* t, tflac_u32 channel, tflac_u32 stride, const tflac_s16* samples, void* nothing) {
    tflac_s32* TFLAC_RESTRICT residuals_0 = TFLAC_ASSUME_ALIGNED(t->residuals[0], 16);
    tflac_u32 i = 0;
    tflac_u32 j = 0;

    tflac_u32 wasted_bits = 0;
    tflac_u32 non_constant = 0;
    tflac_u32 min_found = 0;

    (void)channel;
    (void)nothing;

    while(i < t->cur_blocksize) {
        residuals_0[i] = (tflac_s32)samples[j];

        wasted_bits = tflac_wasted_bits(residuals_0[i], t->subframe_bitdepth);
        if(wasted_bits < t->wasted_bits) t->wasted_bits = wasted_bits;
        non_constant |= (tflac_u32)residuals_0[i] ^ (tflac_u32)residuals_0[0];
        min_found |= residuals_0[i] == INT32_MIN;

        i++;
        j += stride;
    }
    t->subframe_bitdepth = t->bitdepth;
    t->constant = !non_constant;
    t->wasted_bits %= t->subframe_bitdepth;
    t->residual_errors[0] = min_found ? TFLAC_U64_MAX : TFLAC_U64_ZERO;
}

TFLAC_PRIVATE void tflac_stereo_decorrelate_independent_int32(tflac* t, tflac_u32 channel, tflac_u32 stride, const tflac_s32* samples, void* nothing) {
    tflac_s32* TFLAC_RESTRICT residuals_0 = TFLAC_ASSUME_ALIGNED(t->residuals[0], 16);
    tflac_u32 i = 0;
    tflac_u32 j = 0;

    tflac_u32 wasted_bits = 0;
    tflac_u32 non_constant = 0;
    tflac_u32 min_found = 0;

    (void)channel;
    (void)nothing;

    while(i < t->cur_blocksize) {
        residuals_0[i] = samples[j];

        wasted_bits = tflac_wasted_bits(residuals_0[i], t->subframe_bitdepth);
        if(wasted_bits < t->wasted_bits) t->wasted_bits = wasted_bits;
        non_constant |= (tflac_u32)residuals_0[i] ^ (tflac_u32)residuals_0[0];
        min_found |= residuals_0[i] == INT32_MIN;

        i++;
        j += stride;
    }
    t->subframe_bitdepth = t->bitdepth;
    t->constant = !non_constant;
    t->wasted_bits %= t->subframe_bitdepth;
    t->residual_errors[0] = min_found ? TFLAC_U64_MAX : TFLAC_U64_ZERO;
}

TFLAC_PRIVATE void tflac_stereo_decorrelate_left_side_int16(tflac* t, tflac_u32 channel, tflac_u32 stride, const tflac_s16* left, const tflac_s16* right) {
    tflac_s32* TFLAC_RESTRICT residuals_0 = TFLAC_ASSUME_ALIGNED(t->residuals[0], 16);
    tflac_u32 i = 0;

    tflac_u32 l = 0;
    tflac_u32 r = 0;

    tflac_u32 wasted_bits = 0;
    tflac_u32 non_constant = 0;
    tflac_u32 min_found = 0;

    if(channel == 0) {
        t->subframe_bitdepth = t->bitdepth;
        while(i < t->cur_blocksize) {
            residuals_0[i] = (tflac_s32)left[l];

            wasted_bits = tflac_wasted_bits(residuals_0[i], t->subframe_bitdepth);
            if(wasted_bits < t->wasted_bits) t->wasted_bits = wasted_bits;
            non_constant |= (tflac_u32)residuals_0[i] ^ (tflac_u32)residuals_0[0];
            min_found |= residuals_0[i] == INT32_MIN;

            i++;
            l += stride;
        }
    } else {
        t->subframe_bitdepth = t->bitdepth + 1;
        while(i < t->cur_blocksize) {
            residuals_0[i] = ((tflac_s32)left[l]) - ((tflac_s32)right[r]);

            wasted_bits = tflac_wasted_bits(residuals_0[i], t->subframe_bitdepth);
            if(wasted_bits < t->wasted_bits) t->wasted_bits = wasted_bits;
            non_constant |= (tflac_u32)residuals_0[i] ^ (tflac_u32)residuals_0[0];
            min_found |= residuals_0[i] == INT32_MIN;

            i++;
            l += stride;
            r += stride;
        }
    }
    t->constant = !non_constant;
    t->wasted_bits %= t->subframe_bitdepth;
    t->residual_errors[0] = min_found ? TFLAC_U64_MAX : TFLAC_U64_ZERO;
}

TFLAC_PRIVATE void tflac_stereo_decorrelate_left_side_int32(tflac* t, tflac_u32 channel, tflac_u32 stride, const tflac_s32* left, const tflac_s32* right) {
    tflac_s32* TFLAC_RESTRICT residuals_0 = TFLAC_ASSUME_ALIGNED(t->residuals[0], 16);
    tflac_u32 i = 0;

    tflac_u32 l = 0;
    tflac_u32 r = 0;

    tflac_u32 wasted_bits = 0;
    tflac_u32 non_constant = 0;
    tflac_u32 min_found = 0;

    if(channel == 0) {
        t->subframe_bitdepth = t->bitdepth;
        while(i < t->cur_blocksize) {
            residuals_0[i] = left[l];

            wasted_bits = tflac_wasted_bits(residuals_0[i], t->subframe_bitdepth);
            if(wasted_bits < t->wasted_bits) t->wasted_bits = wasted_bits;
            non_constant |= (tflac_u32)residuals_0[i] ^ (tflac_u32)residuals_0[0];
            min_found |= residuals_0[i] == INT32_MIN;

            i++;
            l += stride;
        }
    } else {
        t->subframe_bitdepth = t->bitdepth + 1;
        while(i < t->cur_blocksize) {
            residuals_0[i] = left[l] - right[r];

            wasted_bits = tflac_wasted_bits(residuals_0[i], t->subframe_bitdepth);
            if(wasted_bits < t->wasted_bits) t->wasted_bits = wasted_bits;
            non_constant |= (tflac_u32)residuals_0[i] ^ (tflac_u32)residuals_0[0];
            min_found |= residuals_0[i] == INT32_MIN;

            i++;
            l += stride;
            r += stride;
        }
    }
    t->constant = !non_constant;
    t->wasted_bits %= t->subframe_bitdepth;
    t->residual_errors[0] = min_found ? TFLAC_U64_MAX : TFLAC_U64_ZERO;
}


TFLAC_PRIVATE void tflac_stereo_decorrelate_side_right_int16(tflac* t, tflac_u32 channel, tflac_u32 stride, const tflac_s16* left, const tflac_s16* right) {
    tflac_s32* TFLAC_RESTRICT residuals_0 = TFLAC_ASSUME_ALIGNED(t->residuals[0], 16);
    tflac_u32 i = 0;

    tflac_u32 l = 0;
    tflac_u32 r = 0;

    tflac_u32 wasted_bits = 0;
    tflac_u32 non_constant = 0;
    tflac_u32 min_found = 0;

    if(channel == 1) {
        t->subframe_bitdepth = t->bitdepth;
        while(i < t->cur_blocksize) {
            residuals_0[i] = (tflac_s32)right[r];

            wasted_bits = tflac_wasted_bits(residuals_0[i], t->subframe_bitdepth);
            if(wasted_bits < t->wasted_bits) t->wasted_bits = wasted_bits;
            non_constant |= (tflac_u32)residuals_0[i] ^ (tflac_u32)residuals_0[0];
            min_found |= residuals_0[i] == INT32_MIN;

            i++;
            r += stride;
        }
    } else {
        t->subframe_bitdepth = t->bitdepth + 1;
        while(i < t->cur_blocksize) {
            residuals_0[i] = ((tflac_s32)left[l]) - ((tflac_s32)right[r]);

            wasted_bits = tflac_wasted_bits(residuals_0[i], t->subframe_bitdepth);
            if(wasted_bits < t->wasted_bits) t->wasted_bits = wasted_bits;
            non_constant |= (tflac_u32)residuals_0[i] ^ (tflac_u32)residuals_0[0];
            min_found |= residuals_0[i] == INT32_MIN;

            i++;
            l += stride;
            r += stride;
        }
    }
    t->constant = !non_constant;
    t->wasted_bits %= t->subframe_bitdepth;
    t->residual_errors[0] = min_found ? TFLAC_U64_MAX : TFLAC_U64_ZERO;
}

TFLAC_PRIVATE void tflac_stereo_decorrelate_side_right_int32(tflac* t, tflac_u32 channel, tflac_u32 stride, const tflac_s32* left, const tflac_s32* right) {
    tflac_s32* TFLAC_RESTRICT residuals_0 = TFLAC_ASSUME_ALIGNED(t->residuals[0], 16);
    tflac_u32 i = 0;

    tflac_u32 l = 0;
    tflac_u32 r = 0;

    tflac_u32 wasted_bits = 0;
    tflac_u32 non_constant = 0;
    tflac_u32 min_found = 0;

    if(channel == 1) {
        t->subframe_bitdepth = t->bitdepth;
        while(i < t->cur_blocksize) {
            residuals_0[i] = right[r];

            wasted_bits = tflac_wasted_bits(residuals_0[i], t->subframe_bitdepth);
            if(wasted_bits < t->wasted_bits) t->wasted_bits = wasted_bits;
            non_constant |= (tflac_u32)residuals_0[i] ^ (tflac_u32)residuals_0[0];
            min_found |= residuals_0[i] == INT32_MIN;

            i++;
            r += stride;
        }
    } else {
        t->subframe_bitdepth = t->bitdepth + 1;
        while(i < t->cur_blocksize) {
            residuals_0[i] = left[l] - right[r];

            wasted_bits = tflac_wasted_bits(residuals_0[i], t->subframe_bitdepth);
            if(wasted_bits < t->wasted_bits) t->wasted_bits = wasted_bits;
            non_constant |= (tflac_u32)residuals_0[i] ^ (tflac_u32)residuals_0[0];
            min_found |= residuals_0[i] == INT32_MIN;

            i++;
            l += stride;
            r += stride;
        }
    }
    t->constant = !non_constant;
    t->wasted_bits %= t->subframe_bitdepth;
    t->residual_errors[0] = min_found ? TFLAC_U64_MAX : TFLAC_U64_ZERO;
}

TFLAC_PRIVATE void tflac_stereo_decorrelate_mid_side_int16(tflac* t, tflac_u32 channel, tflac_u32 stride, const tflac_s16* left, const tflac_s16* right) {
    tflac_s32* TFLAC_RESTRICT residuals_0 = TFLAC_ASSUME_ALIGNED(t->residuals[0], 16);
    tflac_u32 i = 0;

    tflac_u32 l = 0;
    tflac_u32 r = 0;

    tflac_u32 wasted_bits = 0;
    tflac_u32 non_constant = 0;
    tflac_u32 min_found = 0;

    if(channel == 0) {
        t->subframe_bitdepth = t->bitdepth;
        while(i < t->cur_blocksize) {
            residuals_0[i] = (((tflac_s32)left[l]) + ((tflac_s32)right[r])) >> 1;

            wasted_bits = tflac_wasted_bits(residuals_0[i], t->subframe_bitdepth);
            if(wasted_bits < t->wasted_bits) t->wasted_bits = wasted_bits;
            non_constant |= (tflac_u32)residuals_0[i] ^ (tflac_u32)residuals_0[0];
            min_found |= residuals_0[i] == INT32_MIN;

            i++;
            l += stride;
            r += stride;
        }
    } else {
        t->subframe_bitdepth = t->bitdepth + 1;
        while(i < t->cur_blocksize) {
            residuals_0[i] = ((tflac_s32)left[l]) - ((tflac_s32)right[r]);

            wasted_bits = tflac_wasted_bits(residuals_0[i], t->subframe_bitdepth);
            if(wasted_bits < t->wasted_bits) t->wasted_bits = wasted_bits;
            non_constant |= (tflac_u32)residuals_0[i] ^ (tflac_u32)residuals_0[0];
            min_found |= residuals_0[i] == INT32_MIN;

            i++;

            l += stride;
            r += stride;
        }
    }
    t->constant = !non_constant;
    t->wasted_bits %= t->subframe_bitdepth;
    t->residual_errors[0] = min_found ? TFLAC_U64_MAX : TFLAC_U64_ZERO;
}

TFLAC_PRIVATE void tflac_stereo_decorrelate_mid_side_int32(tflac* t, tflac_u32 channel, tflac_u32 stride, const tflac_s32* left, const tflac_s32* right) {
    tflac_s32* TFLAC_RESTRICT residuals_0 = TFLAC_ASSUME_ALIGNED(t->residuals[0], 16);
    tflac_u32 i = 0;

    tflac_u32 l = 0;
    tflac_u32 r = 0;

    tflac_u32 wasted_bits = 0;
    tflac_u32 non_constant = 0;
    tflac_u32 min_found = 0;

    if(channel == 0) {
        t->subframe_bitdepth = t->bitdepth;
        while(i < t->cur_blocksize) {
            residuals_0[i] = (left[l] + right[r]) >> 1;

            wasted_bits = tflac_wasted_bits(residuals_0[i], t->subframe_bitdepth);
            if(wasted_bits < t->wasted_bits) t->wasted_bits = wasted_bits;
            non_constant |= (tflac_u32)residuals_0[i] ^ (tflac_u32)residuals_0[0];
            min_found |= residuals_0[i] == INT32_MIN;

            i++;
            l += stride;
            r += stride;
        }
    } else {
        t->subframe_bitdepth = t->bitdepth + 1;
        while(i < t->cur_blocksize) {
            residuals_0[i] = left[l] - right[r];

            wasted_bits = tflac_wasted_bits(residuals_0[i], t->subframe_bitdepth);
            if(wasted_bits < t->wasted_bits) t->wasted_bits = wasted_bits;
            non_constant |= (tflac_u32)residuals_0[i] ^ (tflac_u32)residuals_0[0];
            min_found |= residuals_0[i] == INT32_MIN;

            i++;
            l += stride;
            r += stride;
        }
    }
    t->constant = !non_constant;
    t->wasted_bits %= t->subframe_bitdepth;
    t->residual_errors[0] = min_found ? TFLAC_U64_MAX : TFLAC_U64_ZERO;
}

TFLAC_PRIVATE void tflac_stereo_decorrelate_int16_planar(tflac* t, tflac_u32 channel, const tflac_s16** samples) {
    switch( (enum TFLAC_CHANNEL_MODE)t->channel_mode) {
        case TFLAC_CHANNEL_INDEPENDENT: tflac_stereo_decorrelate_independent_int16(t, channel, 1, samples[channel], NULL); break;
        case TFLAC_CHANNEL_LEFT_SIDE:   tflac_stereo_decorrelate_left_side_int16(t, channel, 1, samples[0], samples[1]); break;
        case TFLAC_CHANNEL_SIDE_RIGHT:  tflac_stereo_decorrelate_side_right_int16(t, channel, 1, samples[0], samples[1]); break;
        case TFLAC_CHANNEL_MID_SIDE:     tflac_stereo_decorrelate_mid_side_int16(t, channel, 1, samples[0], samples[1]); break;
        default: break;
    }
}

TFLAC_PRIVATE void tflac_stereo_decorrelate_int16_interleaved(tflac* t, tflac_u32 channel, const tflac_s16* samples) {
    switch( (enum TFLAC_CHANNEL_MODE)t->channel_mode) {
        case TFLAC_CHANNEL_INDEPENDENT: tflac_stereo_decorrelate_independent_int16(t, channel, t->channels, &samples[channel], NULL); break;
        case TFLAC_CHANNEL_LEFT_SIDE:   tflac_stereo_decorrelate_left_side_int16(t, channel, t->channels, &samples[0], &samples[1]); break;
        case TFLAC_CHANNEL_SIDE_RIGHT:  tflac_stereo_decorrelate_side_right_int16(t, channel, t->channels, &samples[0], &samples[1]); break;
        case TFLAC_CHANNEL_MID_SIDE:    tflac_stereo_decorrelate_mid_side_int16(t, channel, t->channels, &samples[0], &samples[1]); break;
        default: break;
    }
}

TFLAC_PRIVATE void tflac_stereo_decorrelate_int32_planar(tflac* t, tflac_u32 channel, const tflac_s32** samples) {
    switch( (enum TFLAC_CHANNEL_MODE)t->channel_mode) {
        case TFLAC_CHANNEL_INDEPENDENT: tflac_stereo_decorrelate_independent_int32(t, channel, 1, samples[channel], NULL); break;
        case TFLAC_CHANNEL_LEFT_SIDE:   tflac_stereo_decorrelate_left_side_int32(t, channel, 1, samples[0], samples[1]); break;
        case TFLAC_CHANNEL_SIDE_RIGHT:  tflac_stereo_decorrelate_side_right_int32(t, channel, 1, samples[0], samples[1]); break;
        case TFLAC_CHANNEL_MID_SIDE:    tflac_stereo_decorrelate_mid_side_int32(t, channel, 1, samples[0], samples[1]); break;
        default: break;
    }
}

TFLAC_PRIVATE void tflac_stereo_decorrelate_int32_interleaved(tflac* t, tflac_u32 channel, const tflac_s32* samples) {
    switch( (enum TFLAC_CHANNEL_MODE)t->channel_mode) {
        case TFLAC_CHANNEL_INDEPENDENT: tflac_stereo_decorrelate_independent_int32(t, channel, t->channels, &samples[channel], NULL); break;
        case TFLAC_CHANNEL_LEFT_SIDE:   tflac_stereo_decorrelate_left_side_int32(t, channel, t->channels, &samples[0], &samples[1]); break;
        case TFLAC_CHANNEL_SIDE_RIGHT:  tflac_stereo_decorrelate_side_right_int32(t, channel, t->channels, &samples[0], &samples[1]); break;
        case TFLAC_CHANNEL_MID_SIDE:    tflac_stereo_decorrelate_mid_side_int32(t, channel, t->channels, &samples[0], &samples[1]); break;
        default: break;
    }
}

TFLAC_PRIVATE void tflac_cfr_order0_std(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {

    tflac_u32 i = 0;
    const tflac_s32* TFLAC_RESTRICT samples = TFLAC_ASSUME_ALIGNED(_samples, 16);
    tflac_u32 residual_abs = 0;
    tflac_u64 residual_err;

    residual_err = TFLAC_U64_ZERO;

    for(i=4;i<blocksize;i++) {
        residual_abs = (tflac_u32)tflac_s32_abs(samples[i]);
        TFLAC_U64_ADD_WORD(residual_err,residual_abs);
    }

    *residual_error = residual_err;
    (void)_residuals;
}


TFLAC_PRIVATE void tflac_cfr_order1_std(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {
    tflac_u32 i = 0;

    const tflac_s32* TFLAC_RESTRICT samples = TFLAC_ASSUME_ALIGNED(_samples, 16);

    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);
    tflac_u32 residual_abs = 0;
    tflac_u64 residual_err;

    residual_err = TFLAC_U64_ZERO;

    residuals[0] = samples[0];
    residuals[1] = samples[1] - samples[0];
    residuals[2] = samples[2] - samples[1];
    residuals[3] = samples[3] - samples[2];

    for(i=4;i<blocksize;i++) {
        residuals[i] = samples[i] - samples[i-1];
        residual_abs = (tflac_u32)tflac_s32_abs(residuals[i]);
        TFLAC_U64_ADD_WORD(residual_err, residual_abs);
    }

    *residual_error = residual_err;
}

TFLAC_PRIVATE void tflac_cfr_order2_std(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {
    tflac_u32 i = 0;

    const tflac_s32* TFLAC_RESTRICT samples = TFLAC_ASSUME_ALIGNED(_samples, 16);

    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);
    tflac_u32 residual_abs = 0;
    tflac_u64 residual_err;
    residual_err = TFLAC_U64_ZERO;

    residuals[0] = samples[0];
    residuals[1] = samples[1];
    residuals[2] = samples[2] - (2 * samples[1]) - (-1 * samples[0]);
    residuals[3] = samples[3] - (2 * samples[2]) - (-1 * samples[1]);;

    for(i=4;i<blocksize;i++) {
        residuals[i] = samples[i] - (2 * samples[i-1]) - (-1 * samples[i-2] );
        residual_abs = (tflac_u32)tflac_s32_abs(residuals[i]);
        TFLAC_U64_ADD_WORD(residual_err, residual_abs);
    }

    *residual_error = residual_err;
}

TFLAC_PRIVATE void tflac_cfr_order3_std(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {
    tflac_u32 i = 0;

    const tflac_s32* TFLAC_RESTRICT samples = TFLAC_ASSUME_ALIGNED(_samples, 16);
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_u32 residual_abs = 0;
    tflac_u64 residual_err;
    residual_err = TFLAC_U64_ZERO;

    residuals[0] = samples[0];
    residuals[1] = samples[1];
    residuals[2] = samples[2];
    residuals[3] = samples[3] - (3 * samples[2]) - (-3 * samples[1]) - samples[0];

    for(i=4;i<blocksize;i++) {
        residuals[i] = samples[i] - (3 * samples[i-1]) - (-3 * samples[i-2]) - samples[i-3];
        residual_abs = (tflac_u32)tflac_s32_abs(residuals[i]);
        TFLAC_U64_ADD_WORD(residual_err, residual_abs);
    }

    *residual_error = residual_err;
}

TFLAC_PRIVATE void tflac_cfr_order4_std(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {
    tflac_u32 i = 0;

    const tflac_s32* TFLAC_RESTRICT samples = TFLAC_ASSUME_ALIGNED(_samples, 16);
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_u32 residual_abs = 0;
    tflac_u64 residual_err;
    residual_err = TFLAC_U64_ZERO;

    residuals[0] = samples[0];
    residuals[1] = samples[1];
    residuals[2] = samples[2];
    residuals[3] = samples[3];

    for(i=4;i<blocksize;i++) {
        residuals[i] = samples[i] - (4 * samples[i-1]) - (-6 * samples[i-2]) - (4 * samples[i-3]) - (-1 * samples[i-4]);
        residual_abs = (tflac_u32)tflac_s32_abs(residuals[i]);
        TFLAC_U64_ADD_WORD(residual_err, residual_abs);
    }

    *residual_error = residual_err;
}

#if defined(TFLAC_ENABLE_SSE2) || defined(TFLAC_ENABLE_SSSE3) || defined(TFLAC_ENABLE_SSE4_1)

#ifdef TFLAC_32BIT_ONLY
#define TFLAC_SSE_ADD64(d,m) \
    do { \
        tflac_u32 u32val = ((tflac_u32)_mm_cvtsi128_si32(m)); \
        d.hi += (d.lo += u32val) < u32val; \
        d.hi += ((tflac_u32)_mm_cvtsi128_si32(_mm_shuffle_epi32(m,0x55))); \
    } while(0)
#else

#ifdef TFLAC_X64
#define TFLAC_SSE_ADD64(d,m) \
    d += ((tflac_u64)_mm_cvtsi128_si64(m));
#else
#define TFLAC_SSE_ADD64(d,m) \
    d += ((tflac_u64)_mm_cvtsi128_si32(m)); \
    d += (((tflac_u64)_mm_cvtsi128_si32(_mm_shuffle_epi32(m,0x55))) << 32);
#endif /* X64 */
#endif /* 32BIT */

#endif

#ifdef TFLAC_ENABLE_SSE2

TFLAC_PRIVATE void tflac_cfr_order0_sse2(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {

    const tflac_s32* TFLAC_RESTRICT samples0 = TFLAC_ASSUME_ALIGNED(_samples, 16);
    const __m128i zero = _mm_setzero_si128();

    __m128i sum = _mm_setzero_si128();
    tflac_u64 residual_err;
    tflac_u32 len = blocksize - 4;

    tflac_u32 residual_abs = 0;

    residual_err = TFLAC_U64_ZERO;

    samples0 += 4;

    while(len >= 4) {
        __m128i samples = _mm_load_si128((const __m128i *)samples0);
        __m128i masks   = _mm_srai_epi32(samples,31);

        samples = _mm_xor_si128(samples, masks);
        samples = _mm_sub_epi32(samples, masks);

        sum = _mm_add_epi64(sum, _mm_unpacklo_epi32(samples, zero));
        sum = _mm_add_epi64(sum, _mm_unpackhi_epi32(samples, zero));

        samples0 += 4;
        len -= 4;
    }

    sum = _mm_add_epi64(sum, _mm_shuffle_epi32(sum,_MM_SHUFFLE(1,0,3,2)));
    TFLAC_SSE_ADD64(residual_err,sum);

    while(len--) {
        residual_abs = (tflac_u32)tflac_s32_abs(*samples0);
        TFLAC_U64_ADD_WORD(residual_err,residual_abs);
        samples0++;
    }

    *residual_error = residual_err;
    (void)_residuals;
}

TFLAC_PRIVATE void tflac_cfr_order1_sse2(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {
    const tflac_s32* TFLAC_RESTRICT samples0 = TFLAC_ASSUME_ALIGNED(_samples, 16);
    const tflac_s32* TFLAC_RESTRICT samples1 = _samples;
    const __m128i zero = _mm_setzero_si128();

    tflac_u32 len = blocksize - 4;
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_u32 residual_abs = 0;
    tflac_s32 residual = 0;
    tflac_u64 residual_err;
    __m128i sum = _mm_setzero_si128();

    residual_err = TFLAC_U64_ZERO;

    *residuals++ = *samples0++;
    *residuals++ = *samples0++ - *samples1++;
    *residuals++ = *samples0++ - *samples1++;
    *residuals++ = *samples0++ - *samples1++;

    while(len > 4) {
        __m128i masks;
        __m128i msamples0 = _mm_load_si128((const __m128i *)samples0);
        __m128i msamples1 = _mm_loadu_si128((const __m128i *)samples1);

        msamples0 = _mm_sub_epi32(msamples0, msamples1);

        _mm_store_si128( (__m128i*)residuals, msamples0);

        masks = _mm_srai_epi32(msamples0,31);

        msamples0 = _mm_xor_si128(msamples0, masks);
        msamples0 = _mm_sub_epi32(msamples0, masks);

        sum = _mm_add_epi64(sum, _mm_unpacklo_epi32(msamples0, zero));
        sum = _mm_add_epi64(sum, _mm_unpackhi_epi32(msamples0, zero));

        residuals += 4;
        samples0 += 4;
        samples1 += 4;
        len -= 4;
    }

    sum = _mm_add_epi64(sum, _mm_shuffle_epi32(sum,_MM_SHUFFLE(1,0,3,2)));
    TFLAC_SSE_ADD64(residual_err,sum);

    while(len--) {
        residual = *samples0++ - *samples1++;
        *residuals++ = residual;

        residual_abs = (tflac_u32)tflac_s32_abs(residual);
        TFLAC_U64_ADD_WORD(residual_err,residual_abs);
    }

    *residual_error = residual_err;
}

TFLAC_PRIVATE void tflac_cfr_order2_sse2(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {
    const tflac_s32* TFLAC_RESTRICT samples0 = TFLAC_ASSUME_ALIGNED(_samples, 16);
    const tflac_s32* TFLAC_RESTRICT samples1 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples2 = _samples;
    const __m128i zero = _mm_setzero_si128();

    tflac_u32 len = blocksize - 4;
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_u32 residual_abs = 0;
    tflac_s32 residual = 0;
    tflac_u64 residual_err;
    __m128i sum = _mm_setzero_si128();

    residual_err = TFLAC_U64_ZERO;

    *residuals++ = *samples0++;
    *residuals++ = *samples0++;
    samples1++;

    *residuals++ = (*samples0++) - (2 * (*samples1++)) - (-1 * (*samples2++));
    *residuals++ = (*samples0++) - (2 * (*samples1++)) - (-1 * (*samples2++));

    while(len > 4) {
        __m128i masks;
        __m128i msamples0 = _mm_load_si128((const __m128i *)samples0);
        __m128i msamples1 = _mm_loadu_si128((const __m128i *)samples1);
        __m128i msamples2 = _mm_loadu_si128((const __m128i *)samples2);

        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_add_epi32(msamples0, msamples2);

        _mm_store_si128( (__m128i*)residuals, msamples0);

        masks = _mm_srai_epi32(msamples0,31);

        msamples0 = _mm_xor_si128(msamples0, masks);
        msamples0 = _mm_sub_epi32(msamples0, masks);

        sum = _mm_add_epi64(sum, _mm_unpacklo_epi32(msamples0, zero));
        sum = _mm_add_epi64(sum, _mm_unpackhi_epi32(msamples0, zero));

        residuals += 4;
        samples0 += 4;
        samples1 += 4;
        samples2 += 4;
        len -= 4;
    }

    sum = _mm_add_epi64(sum, _mm_shuffle_epi32(sum,_MM_SHUFFLE(1,0,3,2)));
    TFLAC_SSE_ADD64(residual_err,sum);

    while(len--) {
        residual = (*samples0++) - (2 * (*samples1++)) - (-1 * (*samples2++));

        residual_abs = (tflac_u32)tflac_s32_abs(residual);
        TFLAC_U64_ADD_WORD(residual_err,residual_abs);

        *residuals++ = residual;
    }

    *residual_error = residual_err;
}

TFLAC_PRIVATE void tflac_cfr_order3_sse2(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {
    const tflac_s32* TFLAC_RESTRICT samples0 = TFLAC_ASSUME_ALIGNED(_samples, 16);
    const tflac_s32* TFLAC_RESTRICT samples1 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples2 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples3 = _samples;
    const __m128i zero = _mm_setzero_si128();

    tflac_u32 len = blocksize - 4;
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_u32 residual_abs = 0;
    tflac_s32 residual = 0;
    tflac_u64 residual_err;
    __m128i sum = _mm_setzero_si128();

    residual_err = TFLAC_U64_ZERO;

    *residuals++ = *samples0++;
    *residuals++ = *samples0++;
    *residuals++ = *samples0++;

    samples1++; samples1++;
    samples2++;

    *residuals++ = *samples0++ - (3 * (*samples1++)) - (-3 * (*samples2++)) - *samples3++;

    while(len > 4) {
        __m128i masks;
        __m128i msamples0 = _mm_load_si128((const __m128i *)samples0);
        __m128i msamples1 = _mm_loadu_si128((const __m128i *)samples1);
        __m128i msamples2 = _mm_loadu_si128((const __m128i *)samples2);
        __m128i msamples3 = _mm_loadu_si128((const __m128i *)samples3);

        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_sub_epi32(msamples0, msamples1);

        msamples0 = _mm_add_epi32(msamples0, msamples2);
        msamples0 = _mm_add_epi32(msamples0, msamples2);
        msamples0 = _mm_add_epi32(msamples0, msamples2);

        msamples0 = _mm_sub_epi32(msamples0, msamples3);

        _mm_store_si128( (__m128i*)residuals, msamples0);

        masks = _mm_srai_epi32(msamples0,31);

        msamples0 = _mm_xor_si128(msamples0, masks);
        msamples0 = _mm_sub_epi32(msamples0, masks);

        sum = _mm_add_epi64(sum, _mm_unpacklo_epi32(msamples0, zero));
        sum = _mm_add_epi64(sum, _mm_unpackhi_epi32(msamples0, zero));

        residuals += 4;
        samples0 += 4;
        samples1 += 4;
        samples2 += 4;
        samples3 += 4;
        len -= 4;
    }

    sum = _mm_add_epi64(sum, _mm_shuffle_epi32(sum,_MM_SHUFFLE(1,0,3,2)));
    TFLAC_SSE_ADD64(residual_err,sum);

    while(len--) {
        residual = *samples0++ - (3 * (*samples1++)) - (-3 * (*samples2++)) - *samples3++;

        residual_abs = (tflac_u32)tflac_s32_abs(residual);
        TFLAC_U64_ADD_WORD(residual_err,residual_abs);

        *residuals++ = residual;
    }

    *residual_error = residual_err;
}

TFLAC_PRIVATE void tflac_cfr_order4_sse2(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {
    const tflac_s32* TFLAC_RESTRICT samples0 = TFLAC_ASSUME_ALIGNED(_samples, 16);
    const tflac_s32* TFLAC_RESTRICT samples1 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples2 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples3 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples4 = TFLAC_ASSUME_ALIGNED(_samples, 16);
    const __m128i zero = _mm_setzero_si128();

    tflac_u32 len = blocksize - 4;
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_u32 residual_abs = 0;
    tflac_s32 residual = 0;
    tflac_u64 residual_err;
    __m128i sum = _mm_setzero_si128();

    residual_err = TFLAC_U64_ZERO;

    *residuals++ = *samples0++;
    *residuals++ = *samples0++;
    *residuals++ = *samples0++;
    *residuals++ = *samples0++;

    samples1++; samples1++; samples1++;
    samples2++; samples2++;
    samples3++;

    while(len > 4) {
        __m128i masks;
        __m128i msamples0 = _mm_load_si128((const __m128i *)samples0);
        __m128i msamples1 = _mm_loadu_si128((const __m128i *)samples1);
        __m128i msamples2 = _mm_loadu_si128((const __m128i *)samples2);
        __m128i msamples3 = _mm_loadu_si128((const __m128i *)samples3);
        __m128i msamples4 = _mm_load_si128((const __m128i *)samples4);

        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_sub_epi32(msamples0, msamples1);

        msamples0 = _mm_add_epi32(msamples0, msamples2);
        msamples0 = _mm_add_epi32(msamples0, msamples2);
        msamples0 = _mm_add_epi32(msamples0, msamples2);
        msamples0 = _mm_add_epi32(msamples0, msamples2);
        msamples0 = _mm_add_epi32(msamples0, msamples2);
        msamples0 = _mm_add_epi32(msamples0, msamples2);

        msamples0 = _mm_sub_epi32(msamples0, msamples3);
        msamples0 = _mm_sub_epi32(msamples0, msamples3);
        msamples0 = _mm_sub_epi32(msamples0, msamples3);
        msamples0 = _mm_sub_epi32(msamples0, msamples3);

        msamples0 = _mm_add_epi32(msamples0, msamples4);

        _mm_store_si128( (__m128i*)residuals, msamples0);

        masks = _mm_srai_epi32(msamples0,31);

        msamples0 = _mm_xor_si128(msamples0, masks);
        msamples0 = _mm_sub_epi32(msamples0, masks);

        sum = _mm_add_epi64(sum, _mm_unpacklo_epi32(msamples0, zero));
        sum = _mm_add_epi64(sum, _mm_unpackhi_epi32(msamples0, zero));

        residuals += 4;
        samples0 += 4;
        samples1 += 4;
        samples2 += 4;
        samples3 += 4;
        samples4 += 4;
        len -= 4;
    }

    sum = _mm_add_epi64(sum, _mm_shuffle_epi32(sum,_MM_SHUFFLE(1,0,3,2)));
    TFLAC_SSE_ADD64(residual_err,sum);

    while(len--) {
        residual = *samples0++ - (4 * (*samples1++)) - (-6 * (*samples2++)) - (4 * (*samples3++)) - (-1 * (*samples4++));

        residual_abs = (tflac_u32)tflac_s32_abs(residual);
        TFLAC_U64_ADD_WORD(residual_err,residual_abs);

        *residuals++ = residual;
    }

    *residual_error = residual_err;
}
#endif /* TFLAC_ENABLE_SSE2 */

#ifdef TFLAC_ENABLE_SSSE3
TFLAC_PRIVATE void tflac_cfr_order0_ssse3(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {

    const tflac_s32* TFLAC_RESTRICT samples0 = TFLAC_ASSUME_ALIGNED(_samples, 16);
    const __m128i zero = _mm_setzero_si128();

    __m128i sum = _mm_setzero_si128();
    tflac_u64 residual_err;
    tflac_u32 len = blocksize - 4;

    tflac_u32 residual_abs = 0;

    residual_err = TFLAC_U64_ZERO;

    samples0 += 4;

    while(len >= 4) {
        __m128i samples = _mm_load_si128((const __m128i *)samples0);
        samples = _mm_abs_epi32(samples);

        sum = _mm_add_epi64(sum, _mm_unpacklo_epi32(samples, zero));
        sum = _mm_add_epi64(sum, _mm_unpackhi_epi32(samples, zero));

        samples0 += 4;
        len -= 4;
    }

    sum = _mm_add_epi64(sum, _mm_shuffle_epi32(sum,_MM_SHUFFLE(1,0,3,2)));
    TFLAC_SSE_ADD64(residual_err,sum);

    while(len--) {
        residual_abs = (tflac_u32)tflac_s32_abs(*samples0);
        TFLAC_U64_ADD_WORD(residual_err,residual_abs);
        samples0++;
    }

    *residual_error = residual_err;
    (void)_residuals;
}

TFLAC_PRIVATE void tflac_cfr_order1_ssse3(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {
    const tflac_s32* TFLAC_RESTRICT samples0 = TFLAC_ASSUME_ALIGNED(_samples, 16);
    const tflac_s32* TFLAC_RESTRICT samples1 = _samples;
    const __m128i zero = _mm_setzero_si128();

    tflac_u32 len = blocksize - 4;
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_u32 residual_abs = 0;
    tflac_s32 residual = 0;
    tflac_u64 residual_err;
    __m128i sum = _mm_setzero_si128();

    residual_err = TFLAC_U64_ZERO;

    *residuals++ = *samples0++;
    *residuals++ = *samples0++ - *samples1++;
    *residuals++ = *samples0++ - *samples1++;
    *residuals++ = *samples0++ - *samples1++;

    while(len > 4) {
        __m128i msamples0 = _mm_load_si128((const __m128i *)samples0);
        __m128i msamples1 = _mm_loadu_si128((const __m128i *)samples1);

        msamples0 = _mm_sub_epi32(msamples0, msamples1);

        _mm_store_si128( (__m128i*)residuals, msamples0);

        msamples0 = _mm_abs_epi32(msamples0);

        sum = _mm_add_epi64(sum, _mm_unpacklo_epi32(msamples0, zero));
        sum = _mm_add_epi64(sum, _mm_unpackhi_epi32(msamples0, zero));

        residuals += 4;
        samples0 += 4;
        samples1 += 4;
        len -= 4;
    }

    sum = _mm_add_epi64(sum, _mm_shuffle_epi32(sum,_MM_SHUFFLE(1,0,3,2)));
    TFLAC_SSE_ADD64(residual_err,sum);

    while(len--) {
        residual = *samples0++ - *samples1++;
        *residuals++ = residual;

        residual_abs = (tflac_u32)tflac_s32_abs(residual);
        TFLAC_U64_ADD_WORD(residual_err,residual_abs);
    }

    *residual_error = residual_err;
}

TFLAC_PRIVATE void tflac_cfr_order2_ssse3(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {
    const tflac_s32* TFLAC_RESTRICT samples0 = TFLAC_ASSUME_ALIGNED(_samples, 16);
    const tflac_s32* TFLAC_RESTRICT samples1 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples2 = _samples;
    const __m128i zero = _mm_setzero_si128();

    tflac_u32 len = blocksize - 4;
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_u32 residual_abs = 0;
    tflac_s32 residual = 0;
    tflac_u64 residual_err;
    __m128i sum = _mm_setzero_si128();

    residual_err = TFLAC_U64_ZERO;

    *residuals++ = *samples0++;
    *residuals++ = *samples0++;
    samples1++;

    *residuals++ = (*samples0++) - (2 * (*samples1++)) - (-1 * (*samples2++));
    *residuals++ = (*samples0++) - (2 * (*samples1++)) - (-1 * (*samples2++));

    while(len > 4) {
        __m128i msamples0 = _mm_load_si128((const __m128i *)samples0);
        __m128i msamples1 = _mm_loadu_si128((const __m128i *)samples1);
        __m128i msamples2 = _mm_loadu_si128((const __m128i *)samples2);

        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_add_epi32(msamples0, msamples2);

        _mm_store_si128( (__m128i*)residuals, msamples0);

        msamples0 = _mm_abs_epi32(msamples0);

        sum = _mm_add_epi64(sum, _mm_unpacklo_epi32(msamples0, zero));
        sum = _mm_add_epi64(sum, _mm_unpackhi_epi32(msamples0, zero));

        residuals += 4;
        samples0 += 4;
        samples1 += 4;
        samples2 += 4;
        len -= 4;
    }

    sum = _mm_add_epi64(sum, _mm_shuffle_epi32(sum,_MM_SHUFFLE(1,0,3,2)));
    TFLAC_SSE_ADD64(residual_err,sum);

    while(len--) {
        residual = (*samples0++) - (2 * (*samples1++)) - (-1 * (*samples2++));

        residual_abs = (tflac_u32)tflac_s32_abs(residual);
        TFLAC_U64_ADD_WORD(residual_err,residual_abs);

        *residuals++ = residual;
    }

    *residual_error = residual_err;
}

TFLAC_PRIVATE void tflac_cfr_order3_ssse3(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {
    const tflac_s32* TFLAC_RESTRICT samples0 = TFLAC_ASSUME_ALIGNED(_samples, 16);
    const tflac_s32* TFLAC_RESTRICT samples1 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples2 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples3 = _samples;
    const __m128i zero = _mm_setzero_si128();

    tflac_u32 len = blocksize - 4;
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_u32 residual_abs = 0;
    tflac_s32 residual = 0;
    tflac_u64 residual_err;
    __m128i sum = _mm_setzero_si128();

    residual_err = TFLAC_U64_ZERO;

    *residuals++ = *samples0++;
    *residuals++ = *samples0++;
    *residuals++ = *samples0++;

    samples1++; samples1++;
    samples2++;

    *residuals++ = *samples0++ - (3 * (*samples1++)) - (-3 * (*samples2++)) - *samples3++;

    while(len > 4) {
        __m128i msamples0 = _mm_load_si128((const __m128i *)samples0);
        __m128i msamples1 = _mm_loadu_si128((const __m128i *)samples1);
        __m128i msamples2 = _mm_loadu_si128((const __m128i *)samples2);
        __m128i msamples3 = _mm_loadu_si128((const __m128i *)samples3);

        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_sub_epi32(msamples0, msamples1);

        msamples0 = _mm_add_epi32(msamples0, msamples2);
        msamples0 = _mm_add_epi32(msamples0, msamples2);
        msamples0 = _mm_add_epi32(msamples0, msamples2);

        msamples0 = _mm_sub_epi32(msamples0, msamples3);

        _mm_store_si128( (__m128i*)residuals, msamples0);

        msamples0 = _mm_abs_epi32(msamples0);

        sum = _mm_add_epi64(sum, _mm_unpacklo_epi32(msamples0, zero));
        sum = _mm_add_epi64(sum, _mm_unpackhi_epi32(msamples0, zero));

        residuals += 4;
        samples0 += 4;
        samples1 += 4;
        samples2 += 4;
        samples3 += 4;
        len -= 4;
    }

    sum = _mm_add_epi64(sum, _mm_shuffle_epi32(sum,_MM_SHUFFLE(1,0,3,2)));
    TFLAC_SSE_ADD64(residual_err,sum);

    while(len--) {
        residual = *samples0++ - (3 * (*samples1++)) - (-3 * (*samples2++)) - *samples3++;

        residual_abs = (tflac_u32)tflac_s32_abs(residual);
        TFLAC_U64_ADD_WORD(residual_err,residual_abs);

        *residuals++ = residual;
    }

    *residual_error = residual_err;
}

TFLAC_PRIVATE void tflac_cfr_order4_ssse3(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {
    const tflac_s32* TFLAC_RESTRICT samples0 = TFLAC_ASSUME_ALIGNED(_samples, 16);
    const tflac_s32* TFLAC_RESTRICT samples1 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples2 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples3 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples4 = TFLAC_ASSUME_ALIGNED(_samples, 16);
    const __m128i zero = _mm_setzero_si128();

    tflac_u32 len = blocksize - 4;
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_u32 residual_abs = 0;
    tflac_s32 residual = 0;
    tflac_u64 residual_err;
    __m128i sum = _mm_setzero_si128();

    residual_err = TFLAC_U64_ZERO;

    *residuals++ = *samples0++;
    *residuals++ = *samples0++;
    *residuals++ = *samples0++;
    *residuals++ = *samples0++;

    samples1++; samples1++; samples1++;
    samples2++; samples2++;
    samples3++;

    while(len > 4) {
        __m128i msamples0 = _mm_load_si128((const __m128i *)samples0);
        __m128i msamples1 = _mm_loadu_si128((const __m128i *)samples1);
        __m128i msamples2 = _mm_loadu_si128((const __m128i *)samples2);
        __m128i msamples3 = _mm_loadu_si128((const __m128i *)samples3);
        __m128i msamples4 = _mm_load_si128((const __m128i *)samples4);

        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_sub_epi32(msamples0, msamples1);

        msamples0 = _mm_add_epi32(msamples0, msamples2);
        msamples0 = _mm_add_epi32(msamples0, msamples2);
        msamples0 = _mm_add_epi32(msamples0, msamples2);
        msamples0 = _mm_add_epi32(msamples0, msamples2);
        msamples0 = _mm_add_epi32(msamples0, msamples2);
        msamples0 = _mm_add_epi32(msamples0, msamples2);

        msamples0 = _mm_sub_epi32(msamples0, msamples3);
        msamples0 = _mm_sub_epi32(msamples0, msamples3);
        msamples0 = _mm_sub_epi32(msamples0, msamples3);
        msamples0 = _mm_sub_epi32(msamples0, msamples3);

        msamples0 = _mm_add_epi32(msamples0, msamples4);

        _mm_store_si128( (__m128i*)residuals, msamples0);

        msamples0 = _mm_abs_epi32(msamples0);

        sum = _mm_add_epi64(sum, _mm_unpacklo_epi32(msamples0, zero));
        sum = _mm_add_epi64(sum, _mm_unpackhi_epi32(msamples0, zero));

        residuals += 4;
        samples0 += 4;
        samples1 += 4;
        samples2 += 4;
        samples3 += 4;
        samples4 += 4;
        len -= 4;
    }

    sum = _mm_add_epi64(sum, _mm_shuffle_epi32(sum,_MM_SHUFFLE(1,0,3,2)));
    TFLAC_SSE_ADD64(residual_err,sum);

    while(len--) {
        residual = *samples0++ - (4 * (*samples1++)) - (-6 * (*samples2++)) - (4 * (*samples3++)) - (-1 * (*samples4++));

        residual_abs = (tflac_u32)tflac_s32_abs(residual);
        TFLAC_U64_ADD_WORD(residual_err,residual_abs);

        *residuals++ = residual;
    }

    *residual_error = residual_err;
}
#endif

#ifdef TFLAC_ENABLE_SSE4_1
TFLAC_PRIVATE void tflac_cfr_order0_sse4_1(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {

    tflac_cfr_order0_ssse3(blocksize, _samples, _residuals, residual_error);
}

TFLAC_PRIVATE void tflac_cfr_order1_sse4_1(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {
    tflac_cfr_order1_ssse3(blocksize, _samples, _residuals, residual_error);
}

TFLAC_PRIVATE void tflac_cfr_order2_sse4_1(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {
    const tflac_s32* TFLAC_RESTRICT samples0 = TFLAC_ASSUME_ALIGNED(_samples, 16);
    const tflac_s32* TFLAC_RESTRICT samples1 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples2 = _samples;
    const __m128i zero  = _mm_setzero_si128();
    const __m128i two   = _mm_set1_epi32(2);
    const __m128i neg_one  = _mm_set1_epi32(-1);

    tflac_u32 len = blocksize - 4;
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_u32 residual_abs = 0;
    tflac_s32 residual = 0;
    tflac_u64 residual_err;
    __m128i sum = _mm_setzero_si128();

    residual_err = TFLAC_U64_ZERO;

    *residuals++ = *samples0++;
    *residuals++ = *samples0++;
    samples1++;

    *residuals++ = (*samples0++) - (2 * (*samples1++)) - (-1 * (*samples2++));
    *residuals++ = (*samples0++) - (2 * (*samples1++)) - (-1 * (*samples2++));

    while(len > 4) {
        __m128i msamples0 = _mm_load_si128((const __m128i *)samples0);
        __m128i msamples1 = _mm_loadu_si128((const __m128i *)samples1);
        __m128i msamples2 = _mm_loadu_si128((const __m128i *)samples2);

        msamples1 = _mm_mullo_epi32(msamples1,two);
        msamples2 = _mm_mullo_epi32(msamples2,neg_one);

        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_sub_epi32(msamples0, msamples2);

        _mm_store_si128( (__m128i*)residuals, msamples0);

        msamples0 = _mm_abs_epi32(msamples0);

        sum = _mm_add_epi64(sum, _mm_unpacklo_epi32(msamples0, zero));
        sum = _mm_add_epi64(sum, _mm_unpackhi_epi32(msamples0, zero));

        residuals += 4;
        samples0 += 4;
        samples1 += 4;
        samples2 += 4;
        len -= 4;
    }

    sum = _mm_add_epi64(sum, _mm_shuffle_epi32(sum,_MM_SHUFFLE(1,0,3,2)));
    TFLAC_SSE_ADD64(residual_err,sum);

    while(len--) {
        residual = (*samples0++) - (2 * (*samples1++)) - (-1 * (*samples2++));

        residual_abs = (tflac_u32)tflac_s32_abs(residual);
        TFLAC_U64_ADD_WORD(residual_err,residual_abs);

        *residuals++ = residual;
    }

    *residual_error = residual_err;
}

TFLAC_PRIVATE void tflac_cfr_order3_sse4_1(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {
    const tflac_s32* TFLAC_RESTRICT samples0 = TFLAC_ASSUME_ALIGNED(_samples, 16);
    const tflac_s32* TFLAC_RESTRICT samples1 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples2 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples3 = _samples;
    const __m128i zero = _mm_setzero_si128();
    const __m128i three = _mm_set1_epi32(3);
    const __m128i neg_three = _mm_set1_epi32(-3);

    tflac_u32 len = blocksize - 4;
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_u32 residual_abs = 0;
    tflac_s32 residual = 0;
    tflac_u64 residual_err;
    __m128i sum = _mm_setzero_si128();

    residual_err = TFLAC_U64_ZERO;

    *residuals++ = *samples0++;
    *residuals++ = *samples0++;
    *residuals++ = *samples0++;

    samples1++; samples1++;
    samples2++;

    *residuals++ = *samples0++ - (3 * (*samples1++)) - (-3 * (*samples2++)) - *samples3++;

    while(len > 4) {
        __m128i msamples0 = _mm_load_si128((const __m128i *)samples0);
        __m128i msamples1 = _mm_loadu_si128((const __m128i *)samples1);
        __m128i msamples2 = _mm_loadu_si128((const __m128i *)samples2);
        __m128i msamples3 = _mm_loadu_si128((const __m128i *)samples3);

        msamples1 = _mm_mullo_epi32(msamples1, three);
        msamples2 = _mm_mullo_epi32(msamples2, neg_three);

        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_sub_epi32(msamples0, msamples2);
        msamples0 = _mm_sub_epi32(msamples0, msamples3);

        _mm_store_si128( (__m128i*)residuals, msamples0);

        msamples0 = _mm_abs_epi32(msamples0);

        sum = _mm_add_epi64(sum, _mm_unpacklo_epi32(msamples0, zero));
        sum = _mm_add_epi64(sum, _mm_unpackhi_epi32(msamples0, zero));

        residuals += 4;
        samples0 += 4;
        samples1 += 4;
        samples2 += 4;
        samples3 += 4;
        len -= 4;
    }

    sum = _mm_add_epi64(sum, _mm_shuffle_epi32(sum,_MM_SHUFFLE(1,0,3,2)));
    TFLAC_SSE_ADD64(residual_err,sum);

    while(len--) {
        residual = *samples0++ - (3 * (*samples1++)) - (-3 * (*samples2++)) - *samples3++;

        residual_abs = (tflac_u32)tflac_s32_abs(residual);
        TFLAC_U64_ADD_WORD(residual_err,residual_abs);

        *residuals++ = residual;
    }

    *residual_error = residual_err;
}

TFLAC_PRIVATE void tflac_cfr_order4_sse4_1(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {
    const tflac_s32* TFLAC_RESTRICT samples0 = TFLAC_ASSUME_ALIGNED(_samples, 16);
    const tflac_s32* TFLAC_RESTRICT samples1 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples2 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples3 = _samples;
    const tflac_s32* TFLAC_RESTRICT samples4 = TFLAC_ASSUME_ALIGNED(_samples, 16);
    const __m128i zero = _mm_setzero_si128();
    const __m128i four = _mm_set1_epi32(4);
    const __m128i neg_six = _mm_set1_epi32(-6);
    const __m128i neg_one = _mm_set1_epi32(-1);

    tflac_u32 len = blocksize - 4;
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_u32 residual_abs = 0;
    tflac_s32 residual = 0;
    tflac_u64 residual_err;
    __m128i sum = _mm_setzero_si128();

    residual_err = TFLAC_U64_ZERO;

    *residuals++ = *samples0++;
    *residuals++ = *samples0++;
    *residuals++ = *samples0++;
    *residuals++ = *samples0++;

    samples1++; samples1++; samples1++;
    samples2++; samples2++;
    samples3++;

    while(len > 4) {
        __m128i msamples0 = _mm_load_si128((const __m128i *)samples0);
        __m128i msamples1 = _mm_loadu_si128((const __m128i *)samples1);
        __m128i msamples2 = _mm_loadu_si128((const __m128i *)samples2);
        __m128i msamples3 = _mm_loadu_si128((const __m128i *)samples3);
        __m128i msamples4 = _mm_load_si128((const __m128i *)samples4);

        msamples1 = _mm_mullo_epi32(msamples1, four);
        msamples2 = _mm_mullo_epi32(msamples2, neg_six);
        msamples3 = _mm_mullo_epi32(msamples3, four);
        msamples4 = _mm_mullo_epi32(msamples4, neg_one);

        msamples0 = _mm_sub_epi32(msamples0, msamples1);
        msamples0 = _mm_sub_epi32(msamples0, msamples2);
        msamples0 = _mm_sub_epi32(msamples0, msamples3);
        msamples0 = _mm_sub_epi32(msamples0, msamples4);

        _mm_store_si128( (__m128i*)residuals, msamples0);

        msamples0 = _mm_abs_epi32(msamples0);

        sum = _mm_add_epi64(sum, _mm_unpacklo_epi32(msamples0, zero));
        sum = _mm_add_epi64(sum, _mm_unpackhi_epi32(msamples0, zero));

        residuals += 4;
        samples0 += 4;
        samples1 += 4;
        samples2 += 4;
        samples3 += 4;
        samples4 += 4;
        len -= 4;
    }

    sum = _mm_add_epi64(sum, _mm_shuffle_epi32(sum,_MM_SHUFFLE(1,0,3,2)));
    TFLAC_SSE_ADD64(residual_err,sum);

    while(len--) {
        residual = *samples0++ - (4 * (*samples1++)) - (-6 * (*samples2++)) - (4 * (*samples3++)) - (-1 * (*samples4++));

        residual_abs = (tflac_u32)tflac_s32_abs(residual);
        TFLAC_U64_ADD_WORD(residual_err,residual_abs);

        *residuals++ = residual;
    }

    *residual_error = residual_err;
}
#endif

TFLAC_PRIVATE void tflac_cfr_order1_wide_std(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {

    tflac_u32 i = 0;
    tflac_u32 max_found = 0;
    const tflac_s32* TFLAC_RESTRICT samples = TFLAC_ASSUME_ALIGNED(_samples, 16);
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_s64 sample0;
    tflac_s64 sample1;

    tflac_u64 residual_abs;
    tflac_u64 residual_err;

    residual_abs = TFLAC_U64_ZERO;
    residual_err = TFLAC_U64_ZERO;

    residuals[0] = samples[0];

    for(i=1;i<4;i++) {
        TFLAC_S64_CAST(sample0,samples[i]);
        TFLAC_S64_CAST(sample1,samples[i-1]);

#ifdef TFLAC_32BIT_ONLY
        TFLAC_S64_NEG(sample1);
        TFLAC_S64_ADD(sample0,sample1);
#else
        sample0 = sample0 - sample1;
#endif

        TFLAC_S64_ABS(residual_abs,sample0);

        max_found |= TFLAC_U64_GT_WORD(residual_abs,INT32_MAX);
        TFLAC_S64_CAST32(residuals[i], sample0);
    }

    for(i=4;i<blocksize;i++) {
        TFLAC_S64_CAST(sample0,samples[i]);
        TFLAC_S64_CAST(sample1,samples[i-1]);

#ifdef TFLAC_32BIT_ONLY
        TFLAC_S64_NEG(sample1);
        TFLAC_S64_ADD(sample0,sample1);
#else
        sample0 = sample0 - sample1;
#endif

        TFLAC_S64_ABS(residual_abs,sample0);
        max_found |= TFLAC_U64_GT_WORD(residual_abs,INT32_MAX);
        TFLAC_S64_CAST32(residuals[i], sample0);
        TFLAC_U64_ADD(residual_err,residual_abs);
    }

    *residual_error = max_found ? TFLAC_U64_MAX : residual_err;
}

TFLAC_PRIVATE void tflac_cfr_order2_wide_std(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {

    tflac_u32 i = 0;
    tflac_u32 max_found = 0;
    const tflac_s32* TFLAC_RESTRICT samples = TFLAC_ASSUME_ALIGNED(_samples, 16);
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_s64 sample0;
    tflac_s64 sample1;
    tflac_s64 sample2;

    tflac_u64 residual_abs;
    tflac_u64 residual_err;

    residuals[0] = samples[0];
    residuals[1] = samples[1];

    residual_abs = TFLAC_U64_ZERO;
    residual_err = TFLAC_U64_ZERO;

    for(i=2;i<4;i++) {
        TFLAC_S64_CAST(sample0,samples[i]);
        TFLAC_S64_CAST(sample1,samples[i-1]);
        TFLAC_S64_CAST(sample2,samples[i-2]);

#ifdef TFLAC_32BIT_ONLY
        TFLAC_S64_NEG(sample1);
        TFLAC_S64_ADD(sample0,sample1);
        TFLAC_S64_ADD(sample0,sample1);
        TFLAC_S64_ADD(sample0,sample2);
#else
        sample0 = sample0 - ( 2 * sample1 ) - (-1 * sample2 );
#endif

        TFLAC_S64_ABS(residual_abs,sample0);
        max_found |= TFLAC_U64_GT_WORD(residual_abs,INT32_MAX);
        TFLAC_S64_CAST32(residuals[i], sample0);
    }

    for(i=4;i<blocksize;i++) {
        TFLAC_S64_CAST(sample0,samples[i]);
        TFLAC_S64_CAST(sample1,samples[i-1]);
        TFLAC_S64_CAST(sample2,samples[i-2]);

#ifdef TFLAC_32BIT_ONLY
        TFLAC_S64_NEG(sample1);
        TFLAC_S64_ADD(sample0,sample1);
        TFLAC_S64_ADD(sample0,sample1);
        TFLAC_S64_ADD(sample0,sample2);
#else
        sample0 = sample0 - ( 2 * sample1 ) - (-1 * sample2 );
#endif

        TFLAC_S64_ABS(residual_abs,sample0);
        max_found |= TFLAC_U64_GT_WORD(residual_abs,INT32_MAX);
        TFLAC_S64_CAST32(residuals[i], sample0);
        TFLAC_U64_ADD(residual_err,residual_abs);
    }

    *residual_error = max_found ? TFLAC_U64_MAX : residual_err;
}

TFLAC_PRIVATE void tflac_cfr_order3_wide_std(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {

    tflac_u32 i = 0;
    tflac_u32 max_found = 0;
    const tflac_s32* TFLAC_RESTRICT samples = TFLAC_ASSUME_ALIGNED(_samples, 16);
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_s64 sample0;
    tflac_s64 sample1;
    tflac_s64 sample2;
    tflac_s64 sample3;

    tflac_u64 residual_abs;
    tflac_u64 residual_err;

    residuals[0] = samples[0];
    residuals[1] = samples[1];
    residuals[2] = samples[2];

    residual_abs = TFLAC_U64_ZERO;
    residual_err = TFLAC_U64_ZERO;

    for(i=3;i<4;i++) {
        TFLAC_S64_CAST(sample0,samples[i]);
        TFLAC_S64_CAST(sample1,samples[i-1]);
        TFLAC_S64_CAST(sample2,samples[i-2]);
        TFLAC_S64_CAST(sample3,samples[i-3]);

#ifdef TFLAC_32BIT_ONLY
        TFLAC_S64_NEG(sample1);
        TFLAC_S64_NEG(sample3);
        TFLAC_S64_ADD(sample0,sample1);
        TFLAC_S64_ADD(sample0,sample1);
        TFLAC_S64_ADD(sample0,sample1);
        TFLAC_S64_ADD(sample0,sample2);
        TFLAC_S64_ADD(sample0,sample2);
        TFLAC_S64_ADD(sample0,sample2);
        TFLAC_S64_ADD(sample0,sample3);
#else
        sample0 = sample0 - ( 3 * sample1 ) - (-3 * sample2 ) - sample3;
#endif

        TFLAC_S64_ABS(residual_abs,sample0);
        max_found |= TFLAC_U64_GT_WORD(residual_abs,INT32_MAX);

        TFLAC_S64_CAST32(residuals[i], sample0);
    }

    for(i=4;i<blocksize;i++) {
        TFLAC_S64_CAST(sample0,samples[i]);
        TFLAC_S64_CAST(sample1,samples[i-1]);
        TFLAC_S64_CAST(sample2,samples[i-2]);
        TFLAC_S64_CAST(sample3,samples[i-3]);

#ifdef TFLAC_32BIT_ONLY
        TFLAC_S64_NEG(sample1);
        TFLAC_S64_NEG(sample3);
        TFLAC_S64_ADD(sample0,sample1);
        TFLAC_S64_ADD(sample0,sample1);
        TFLAC_S64_ADD(sample0,sample1);
        TFLAC_S64_ADD(sample0,sample2);
        TFLAC_S64_ADD(sample0,sample2);
        TFLAC_S64_ADD(sample0,sample2);
        TFLAC_S64_ADD(sample0,sample3);
#else
        sample0 = sample0 - ( 3 * sample1 ) - (-3 * sample2 ) - sample3;
#endif

        TFLAC_S64_ABS(residual_abs,sample0);
        max_found |= TFLAC_U64_GT_WORD(residual_abs,INT32_MAX);

        TFLAC_S64_CAST32(residuals[i], sample0);
        TFLAC_U64_ADD(residual_err,residual_abs);
    }

    *residual_error = max_found ? TFLAC_U64_MAX : residual_err;
}

TFLAC_PRIVATE void tflac_cfr_order4_wide_std(
      tflac_u32 blocksize,
      const tflac_s32* TFLAC_RESTRICT _samples,
      tflac_s32* TFLAC_RESTRICT _residuals,
      tflac_u64* TFLAC_RESTRICT residual_error) {

    tflac_u32 i = 0;
    tflac_u32 max_found = 0;
    const tflac_s32* TFLAC_RESTRICT samples = TFLAC_ASSUME_ALIGNED(_samples, 16);
    tflac_s32* TFLAC_RESTRICT residuals = TFLAC_ASSUME_ALIGNED(_residuals, 16);

    tflac_s64 sample0;
    tflac_s64 sample1;
    tflac_s64 sample2;
    tflac_s64 sample3;
    tflac_s64 sample4;

    tflac_u64 residual_abs;
    tflac_u64 residual_err;

    residual_abs = TFLAC_U64_ZERO;
    residual_err = TFLAC_U64_ZERO;

    residuals[0] = samples[0];
    residuals[1] = samples[1];
    residuals[2] = samples[2];
    residuals[3] = samples[3];

    for(i=4;i<blocksize;i++) {
        TFLAC_S64_CAST(sample0,samples[i]);
        TFLAC_S64_CAST(sample1,samples[i-1]);
        TFLAC_S64_CAST(sample2,samples[i-2]);
        TFLAC_S64_CAST(sample3,samples[i-3]);
        TFLAC_S64_CAST(sample4,samples[i-4]);

#ifdef TFLAC_32BIT_ONLY
        TFLAC_S64_NEG(sample1);
        TFLAC_S64_NEG(sample3);

        TFLAC_S64_ADD(sample0,sample1);
        TFLAC_S64_ADD(sample0,sample1);
        TFLAC_S64_ADD(sample0,sample1);
        TFLAC_S64_ADD(sample0,sample1);

        TFLAC_S64_ADD(sample0,sample2);
        TFLAC_S64_ADD(sample0,sample2);
        TFLAC_S64_ADD(sample0,sample2);
        TFLAC_S64_ADD(sample0,sample2);
        TFLAC_S64_ADD(sample0,sample2);
        TFLAC_S64_ADD(sample0,sample2);

        TFLAC_S64_ADD(sample0,sample3);
        TFLAC_S64_ADD(sample0,sample3);
        TFLAC_S64_ADD(sample0,sample3);
        TFLAC_S64_ADD(sample0,sample3);

        TFLAC_S64_ADD(sample0,sample4);
#else
        sample0 = sample0 - (4 * sample1) - (-6 * sample2) - (4 * sample3) - (-1 * sample4);
#endif

        TFLAC_S64_ABS(residual_abs,sample0);
        max_found |= TFLAC_U64_GT_WORD(residual_abs,INT32_MAX);
        TFLAC_S64_CAST32(residuals[i], sample0);
        TFLAC_U64_ADD(residual_err,residual_abs);
    }

    *residual_error = max_found ? TFLAC_U64_MAX : residual_err;
}

TFLAC_PRIVATE void tflac_cfr(tflac *t) {

    t->residual_errors[1] = TFLAC_U64_ZERO;
    t->residual_errors[2] = TFLAC_U64_ZERO;
    t->residual_errors[3] = TFLAC_U64_ZERO;
    t->residual_errors[4] = TFLAC_U64_ZERO;

    if(TFLAC_UNLIKELY(t->cur_blocksize < 5)) {
        /* either the block is short or the samples contain INT32_MIN so we'll just bail */
        t->residual_errors[1] = TFLAC_U64_MAX;
        t->residual_errors[2] = TFLAC_U64_MAX;
        t->residual_errors[3] = TFLAC_U64_MAX;
        t->residual_errors[4] = TFLAC_U64_MAX;
        return;
    }

    if(TFLAC_LIKELY(TFLAC_U64_EQ_WORD(t->residual_errors[0],0))) {
        t->calculate_order[0](t->cur_blocksize,t->residuals[0],NULL           ,&t->residual_errors[0]);
    }
    t->calculate_order[1](t->cur_blocksize,t->residuals[0],t->residuals[1],&t->residual_errors[1]);
    t->calculate_order[2](t->cur_blocksize,t->residuals[0],t->residuals[2],&t->residual_errors[2]);
    t->calculate_order[3](t->cur_blocksize,t->residuals[0],t->residuals[3],&t->residual_errors[3]);
    t->calculate_order[4](t->cur_blocksize,t->residuals[0],t->residuals[4],&t->residual_errors[4]);

    return;
}


TFLAC_PRIVATE
int tflac_encode_residuals(tflac* t, tflac_u8 predictor_order, tflac_u8 partition_order) {
    int r;
    tflac_u32 rice = 0;
    tflac_u32 i = 0;
    tflac_u32 j = 0;

    tflac_u32 res_abs = 0;
    tflac_u32 v = 0;
    tflac_u64 sum;

    tflac_u8 w = (tflac_u8)t->wasted_bits;
    tflac_u32 partition_length = 0;
    tflac_u32 offset = predictor_order;
    tflac_u32 msb = 0;
    tflac_u32 lsb = 0;
    tflac_u32 neg = 0;
    tflac_u32 bits = t->bw.tot;
    const tflac_s32* residuals = TFLAC_ASSUME_ALIGNED(t->residuals[predictor_order], 16);

    sum = TFLAC_U64_ZERO;

    if( (r = tflac_bitwriter_add(&t->bw, 8, (tflac_uint)(0x10 | (predictor_order << 1) | (!!w))) ) != 0) return r;
    if(w) if( (r = tflac_bitwriter_add(&t->bw, w, 1)) != 0) return r;


    for(i=0;i<predictor_order;i++) {
        if( (r = tflac_bitwriter_add(&t->bw, t->subframe_bitdepth - t->wasted_bits, (tflac_u32)residuals[i])) != 0) return r;
    }

    if( (r = tflac_bitwriter_add(&t->bw, 6,
        ( (t->max_rice_value > 14 ? 0x10 : 0x00) ) | partition_order)) != 0) return r;

    for(i=0;i < ( 1U << partition_order) ; i++) {

        partition_length = t->cur_blocksize >> partition_order;
        if(i == 0) partition_length -= (tflac_u32)predictor_order;

        sum = TFLAC_U64_ZERO;
        for(j=0;j<partition_length;j++) {
            res_abs = (tflac_u32)tflac_s32_abs(residuals[j+offset]);
            TFLAC_U64_ADD_WORD(sum, res_abs);
        }

        /* find the rice parameter for this partition */
        rice = 0;
        while( TFLAC_U64_GT_WORD(sum, (partition_length << rice)) ) {
            if(rice == t->max_rice_value) break;
            rice++;
        }

        if(t->max_rice_value > 14) {
            if( (r = tflac_bitwriter_add(&t->bw, 5, rice)) != 0) return r;
        } else {
            if( (r = tflac_bitwriter_add(&t->bw, 4, rice)) != 0) return r;
        }

        for(j=0;j<partition_length;j++) {
            /* the original version is something like:
             * if(residuals[j+offset] < 0) {
             *   v = residuals[j+offset] * -2 - 1;
             * } else {
             *   v = residuals[j+offset] * 2
             * }
             * instead we just find the sign bit, double
             * the absolute value, and subtract the sign bit */
            neg = (tflac_u32)(residuals[j+offset]) >> 31;
            v = ((tflac_u32)tflac_s32_abs(residuals[j+offset])) << 1;
            v -= neg;

            msb = (tflac_u32)(v >> rice);
            lsb = (tflac_u32)(v - (msb << rice));

            if(msb) if( (r = tflac_bitwriter_zeroes(&t->bw, msb)) != 0) return r;
            if( (r = tflac_bitwriter_add(&t->bw, rice + 1, (1 << rice) | lsb)) != 0) return r;
        }

        offset += partition_length;
    }

    /* flush the output */
    if( (r = tflac_bitwriter_flush(&t->bw)) != 0) return r;

    if(t->bw.tot - bits > t->verbatim_subframe_bits) {
        /* we somehow took more space than we would with a verbatim subframe? */
        return -1;
    }

    return 0;
}


TFLAC_PRIVATE
int tflac_encode_subframe_fixed(tflac* t) {
    tflac_u8 i = 0;
    tflac_u8 order = 5;
    tflac_u8 max_order = 4;
    tflac_u64 error;
    tflac_u8 partition_order = t->partition_order;

    error = TFLAC_U64_MAX;

    tflac_cfr(t);

    while( t->cur_blocksize >> t->partition_order <= max_order ) max_order--;

    for(i=0;i<=max_order;i++) {
        if(TFLAC_U64_LT(t->residual_errors[i],error)) {
            error = t->residual_errors[i];
            order = i;
        }
    }

    if(order == max_order+1) return -1;

    return tflac_encode_residuals(t, order, partition_order);
}

TFLAC_PRIVATE
int tflac_encode_subframe(tflac *t, tflac_u8 channel) {
    int r;
    tflac_bitwriter bw = t->bw;

    t->verbatim_subframe_bits = tflac_verbatim_subframe_bits(t->cur_blocksize, t->subframe_bitdepth);

#ifdef TFLAC_DISABLE_COUNTERS
    (void)channel;
#endif

    if(t->enable_constant_subframe && t->constant) {
        if(tflac_encode_subframe_constant(t) == 0) {
#ifndef TFLAC_DISABLE_COUNTERS
            TFLAC_U64_ADD_WORD(t->subframe_type_counts[channel][TFLAC_SUBFRAME_CONSTANT],1);
#endif
            return 0;
        }
        t->bw = bw;
    }

    if(t->enable_fixed_subframe) {
        if(tflac_encode_subframe_fixed(t) == 0) {
#ifndef TFLAC_DISABLE_COUNTERS
            TFLAC_U64_ADD_WORD(t->subframe_type_counts[channel][TFLAC_SUBFRAME_FIXED],1);
#endif
            return 0;
        }
        t->bw = bw;
    }

    r = tflac_encode_subframe_verbatim(t);
#ifndef TFLAC_DISABLE_COUNTERS
    if(r == 0) {
        TFLAC_U64_ADD_WORD(t->subframe_type_counts[channel][TFLAC_SUBFRAME_VERBATIM],1);
    }
#endif
    return r;
}

TFLAC_PRIVATE
int tflac_encode_frame_header(tflac *t) {
    int r;
    tflac_u8 frameno_bytes[6];
    unsigned int i = 0;
    tflac_u8 frameno_len = 0;

    if( (r = tflac_bitwriter_add(&t->bw, 32, t->frame_header)) != 0) return r;

    if(t->frameno < ( (tflac_u32)1 << 7) ) {
        frameno_bytes[0] = t->frameno & 0x7F;
        frameno_len = 1;
    } else if(t->frameno < ((tflac_u32)1 << 11)) {
        frameno_bytes[0] = 0xC0 | ((t->frameno >> 6) & 0x1F);
        frameno_bytes[1] = 0x80 | ((t->frameno     ) & 0x3F);
        frameno_len = 2;
    } else if(t->frameno < ((tflac_u32)1 << 16)) {
        frameno_bytes[0] = 0xE0 | ((t->frameno >> 12) & 0x0F);
        frameno_bytes[1] = 0x80 | ((t->frameno >>  6) & 0x3F);
        frameno_bytes[2] = 0x80 | ((t->frameno      ) & 0x3F);
        frameno_len = 3;
    } else if(t->frameno < ((tflac_u32)1 << 21)) {
        frameno_bytes[0] = 0xF0 | ((t->frameno >> 18) & 0x07);
        frameno_bytes[1] = 0x80 | ((t->frameno >> 12) & 0x3F);
        frameno_bytes[2] = 0x80 | ((t->frameno >>  6) & 0x3F);
        frameno_bytes[3] = 0x80 | ((t->frameno      ) & 0x3F);
        frameno_len = 4;
    } else if(t->frameno < ((tflac_u32)1 << 26)) {
        frameno_bytes[0] = 0xF8 | ((t->frameno >> 24) & 0x03);
        frameno_bytes[1] = 0x80 | ((t->frameno >> 18) & 0x3F);
        frameno_bytes[2] = 0x80 | ((t->frameno >> 12) & 0x3F);
        frameno_bytes[3] = 0x80 | ((t->frameno >>  6) & 0x3F);
        frameno_bytes[4] = 0x80 | ((t->frameno      ) & 0x3F);
        frameno_len = 5;
    } else {
        frameno_bytes[0] = 0xFC | ((t->frameno >> 30) & 0x01);
        frameno_bytes[1] = 0x80 | ((t->frameno >> 24) & 0x3F);
        frameno_bytes[2] = 0x80 | ((t->frameno >> 18) & 0x3F);
        frameno_bytes[3] = 0x80 | ((t->frameno >> 12) & 0x3F);
        frameno_bytes[4] = 0x80 | ((t->frameno >>  6) & 0x3F);
        frameno_bytes[5] = 0x80 | ((t->frameno      ) & 0x3F);
        frameno_len = 6;
    }

    for(i=0;i<frameno_len;i++) {
        if( (r = tflac_bitwriter_add(&t->bw, 8, frameno_bytes[i])) != 0) return r;
    }

    switch( (t->frame_header >> 12) & 0x0F) {
        case 6: {
            if( (r = tflac_bitwriter_add(&t->bw, 8, t->cur_blocksize - 1)) != 0) return r;
            break;
        }
        case 7: {
            if( (r = tflac_bitwriter_add(&t->bw, 16, t->cur_blocksize - 1)) != 0) return r;
            break;
        }
        default: break;
    }

    switch( (t->frame_header >> 8) & 0x0F) {
        case 12: {
            if( (r = tflac_bitwriter_add(&t->bw, 8, t->samplerate / 1000)) != 0) return r;
            break;
        }
        case 13: {
            if( (r = tflac_bitwriter_add(&t->bw, 16, t->samplerate)) != 0) return r;
            break;
        }
        case 14: {
            if( (r = tflac_bitwriter_add(&t->bw, 16, t->samplerate / 10)) != 0) return r;
            break;
        }
        default: break;
    }

    if( (r = tflac_bitwriter_flush(&t->bw)) != 0) return r;
    if( (r = tflac_bitwriter_add(&t->bw, 8, tflac_crc8(t->bw.buffer,t->bw.pos, 0))) != 0) return r;
    return tflac_bitwriter_flush(&t->bw); /* flush to ensure we have our current byte position marked */
}

TFLAC_PUBLIC
void tflac_init(tflac *t) {
    tflac_bitwriter_init(&t->bw);
    tflac_md5_init(&t->md5_ctx);

    t->blocksize = 0;
    t->samplerate = 0;
    t->channels = 0;
    t->bitdepth = 0;
    t->channel_mode = (tflac_u8)TFLAC_CHANNEL_INDEPENDENT;

    t->subframe_bitdepth = 0;
    t->max_rice_value = 0;
    t->min_partition_order = 0;
    t->max_partition_order = 0;
    t->partition_order = 0;

    t->enable_constant_subframe = 1;
    t->enable_fixed_subframe = 1;
    t->enable_md5 = 1;

    t->frame_header = 0;

    t->samplecount = TFLAC_U64_ZERO;
    t->frameno = 0;
    t->verbatim_subframe_bits = 0;
    t->cur_blocksize = 0;
    t->max_frame_len = 0;

    t->min_frame_size = 0;
    t->max_frame_size = 0;

    t->wasted_bits = 0;
    t->constant = 0;

    t->md5_digest[0] = '\0';
    t->md5_digest[1] = '\0';
    t->md5_digest[2] = '\0';
    t->md5_digest[3] = '\0';
    t->md5_digest[4] = '\0';
    t->md5_digest[5] = '\0';
    t->md5_digest[6] = '\0';
    t->md5_digest[7] = '\0';
    t->md5_digest[8] = '\0';
    t->md5_digest[9] = '\0';
    t->md5_digest[10] = '\0';
    t->md5_digest[11] = '\0';
    t->md5_digest[12] = '\0';
    t->md5_digest[13] = '\0';
    t->md5_digest[14] = '\0';
    t->md5_digest[15] = '\0';

    t->calculate_order[0] = tflac_cfr_order0;
    t->calculate_order[1] = tflac_cfr_order1;
    t->calculate_order[2] = tflac_cfr_order2;
    t->calculate_order[3] = tflac_cfr_order3;
    t->calculate_order[4] = tflac_cfr_order4;

    t->residuals[0] = NULL;
    t->residuals[1] = NULL;
    t->residuals[2] = NULL;
    t->residuals[3] = NULL;
    t->residuals[4] = NULL;

#ifndef TFLAC_DISABLE_COUNTERS
    {
        unsigned int i,j;
        for(i=0;i<8;i++) {
            for(j=0;j<TFLAC_SUBFRAME_TYPE_COUNT;j++) {
                t->subframe_type_counts[i][j] = TFLAC_U64_ZERO;
            }
        }
    }
#endif

}

TFLAC_PRIVATE
void tflac_update_frame_header(tflac *t) {
    t->frame_header = UINT32_C(0xFFF8) << 16;

    switch(t->cur_blocksize) {
        case 192:   t->frame_header |= ( UINT32_C(0x01) << 12); break;
        case 576:   t->frame_header |= ( UINT32_C(0x02) << 12); break;
        case 1152:  t->frame_header |= ( UINT32_C(0x03) << 12); break;
        case 2304:  t->frame_header |= ( UINT32_C(0x04) << 12); break;
        case 4608:  t->frame_header |= ( UINT32_C(0x05) << 12); break;
        case 256:   t->frame_header |= ( UINT32_C(0x08) << 12); break;
        case 512:   t->frame_header |= ( UINT32_C(0x09) << 12); break;
        case 1024:  t->frame_header |= ( UINT32_C(0x0A) << 12); break;
        case 2048:  t->frame_header |= ( UINT32_C(0x0B) << 12); break;
        case 4096:  t->frame_header |= ( UINT32_C(0x0C) << 12); break;
        case 8192:  t->frame_header |= ( UINT32_C(0x0D) << 12); break;
        case 16384: t->frame_header |= ( UINT32_C(0x0E) << 12); break;
        case 32768: t->frame_header |= ( UINT32_C(0x0F) << 12); break;
        default: {
            t->frame_header |= t->cur_blocksize <= 256 ? (UINT32_C(0x06) << 12) : (UINT32_C(0x07) << 12);
        }
    }

    switch(t->samplerate) {
        case 882000: t->frame_header |= (UINT32_C(0x01) << 8); break;
        case 176400: t->frame_header |= (UINT32_C(0x02) << 8); break;
        case 192000: t->frame_header |= (UINT32_C(0x03) << 8); break;
        case   8000: t->frame_header |= (UINT32_C(0x04) << 8); break;
        case  16000: t->frame_header |= (UINT32_C(0x05) << 8); break;
        case  22050: t->frame_header |= (UINT32_C(0x06) << 8); break;
        case  24000: t->frame_header |= (UINT32_C(0x07) << 8); break;
        case  32000: t->frame_header |= (UINT32_C(0x08) << 8); break;
        case  44100: t->frame_header |= (UINT32_C(0x09) << 8); break;
        case  48000: t->frame_header |= (UINT32_C(0x0A) << 8); break;
        case  96000: t->frame_header |= (UINT32_C(0x0B) << 8); break;
        default: {
            if(t->samplerate % 1000 == 0) {
                if(t->samplerate / 1000 < 256) {
                    t->frame_header |= (UINT32_C(0x0C) << 8);
                }
            } else if(t->samplerate < 65536) {
                t->frame_header |= (UINT32_C(0x0D) << 8);
            } else if(t->samplerate % 10 == 0) {
                if(t->samplerate / 10 < 65536) {
                    t->frame_header |= (UINT32_C(0x0E) << 8);
                }
            }
        }
    }

    switch((enum TFLAC_CHANNEL_MODE)t->channel_mode) {
        case TFLAC_CHANNEL_INDEPENDENT: t->frame_header |= (t->channels - 1) << 4; break;
        case TFLAC_CHANNEL_LEFT_SIDE: t->frame_header |= (0x08) << 4; break;
        case TFLAC_CHANNEL_SIDE_RIGHT: t->frame_header |= (0x09) << 4; break;
        case TFLAC_CHANNEL_MID_SIDE: t->frame_header |= (0x0A) << 4; break;
        default: break;
    }

    switch(t->bitdepth) {
        case 8:  t->frame_header |= (UINT32_C(1) << 1); break;
        case 12: t->frame_header |= (UINT32_C(2) << 1); break;
        case 16: t->frame_header |= (UINT32_C(4) << 1); break;
        case 20: t->frame_header |= (UINT32_C(5) << 1); break;
        case 24: t->frame_header |= (UINT32_C(6) << 1); break;
        case 32: t->frame_header |= (UINT32_C(7) << 1); break;
        default: break;
    }
}

TFLAC_PUBLIC
int tflac_validate(tflac *t, void* ptr, tflac_u32 len) {
    tflac_u32 res_len = 0;
    tflac_u8* d;
    tflac_uptr p2, p1;

    if(t->blocksize < 16) return -1;
    if(t->blocksize > 65535) return -1;
    if(t->samplerate == 0) return -1;
    if(t->samplerate > 655350) return -1;
    if(t->channels == 0) return -1;
    if(t->channels > 8) return -1;
    if(t->bitdepth == 0) return -1;
    if(t->bitdepth > 32) return -1;

    if(t->channel_mode != TFLAC_CHANNEL_INDEPENDENT) {
        if(t->channels != 2 || t->bitdepth == 32) {
            t->channel_mode = TFLAC_CHANNEL_INDEPENDENT;
        }
    }

    if(t->max_rice_value == 0) {
        if(t->bitdepth <= 16) {
            t->max_rice_value = 14;
        } else {
            t->max_rice_value = 30;
        }
    } else if(t->max_rice_value > 30) {
        return -1;
    }

    if(t->max_partition_order > 15) {
        return -1;
    }

    if(t->min_partition_order > t->max_partition_order) return -1;

    if(len < tflac_size_memory(t->blocksize)) return -1;

    p1 = ((tflac_uptr)ptr);
    p2 = (p1 + 15) & ~(tflac_uptr)UINT32_C(0x0F);

    p2 -= p1; /* we now have an offset rather than an absolute pointer,
    in case things went sideways with type detecion and tflac_uptr
    wound up too small */

    d = (tflac_u8*)ptr;
    d += p2;

    res_len = (15UL + (t->blocksize * 4UL)) & UINT32_C(0xFFFFFFF0);
    t->residuals[0] = (tflac_s32*)(&d[(0 * res_len)]);
    t->residuals[1] = (tflac_s32*)(&d[(1 * res_len)]);
    t->residuals[2] = (tflac_s32*)(&d[(2 * res_len)]);
    t->residuals[3] = (tflac_s32*)(&d[(3 * res_len)]);
    t->residuals[4] = (tflac_s32*)(&d[(4 * res_len)]);

    t->partition_order = t->min_partition_order;
    while( (t->blocksize % (1<<(t->partition_order+1)) == 0) && t->partition_order < t->max_partition_order) {
        t->partition_order++;
    }
    t->cur_blocksize = t->blocksize;

    tflac_update_frame_header(t);

    switch(t->bitdepth) {
        case 32: {
            t->calculate_order[1] = tflac_cfr_order1_wide;
        }
        /* fall-through */
        case 31: {
            t->calculate_order[2] = tflac_cfr_order2_wide;
        }
        /* fall-through */
        case 30: {
            t->calculate_order[3] = tflac_cfr_order3_wide;
        }
        /* fall-through */
        case 29: {
            t->calculate_order[4] = tflac_cfr_order4_wide;
        }
        /* fall-through */
        default: break;
    }

    t->max_frame_len = tflac_max_size_frame(t->cur_blocksize, t->channels, t->bitdepth);

    return 0;
}

TFLAC_PRIVATE
int tflac_encode(tflac* t, const tflac_encode_params* p) {
    tflac_u8 c = 0;
    int r;

    if(t->cur_blocksize != p->blocksize) {
        t->cur_blocksize = p->blocksize;

        t->partition_order = t->min_partition_order;
        while( (t->cur_blocksize % (1<<(t->partition_order+1)) == 0) && t->partition_order < t->max_partition_order) {
            t->partition_order++;
        }

        tflac_update_frame_header(t);
        t->max_frame_len = tflac_max_size_frame(t->cur_blocksize, t->channels, t->bitdepth);
    }

    if(t->enable_md5) p->calculate_md5(t, p->samples);

    tflac_bitwriter_init(&t->bw);
    t->bw.buffer = p->buffer;
    t->bw.len    = p->buffer_len;
    if(t->bw.len > t->max_frame_len) t->bw.len = t->max_frame_len;

    if( (r = tflac_encode_frame_header(t)) != 0) return r;

    for(c=0;c<t->channels;c++) {
        t->residual_errors[0] = TFLAC_U64_ZERO;
        p->decorrelate(t, c, p->samples);
        tflac_rescale_samples(t);
        if( (r = tflac_encode_subframe(t, c)) != 0) return r;
    }
    if( (r = tflac_bitwriter_align(&t->bw)) != 0) return r;
    if( (r = tflac_bitwriter_flush(&t->bw)) != 0) return r;
    if( (r = tflac_bitwriter_add(&t->bw, 16, tflac_crc16(t->bw.buffer,t->bw.pos,0))) != 0) return r;
    if( (r = tflac_bitwriter_flush(&t->bw)) != 0) return r;

    *(p->used) = t->bw.pos;
    if(t->bw.pos < t->min_frame_size || t->min_frame_size == 0) {
        t->min_frame_size = t->bw.pos;
    }

    if(t->bw.pos > t->max_frame_size) {
        t->max_frame_size = t->bw.pos;
    }

    t->frameno++;
    t->frameno &= UINT32_C(0x7FFFFFFF); /* cap to 31 bits */

    TFLAC_U64_ADD_WORD(t->samplecount, t->cur_blocksize);

#ifdef TFLAC_32BIT_ONLY
    t->samplecount.hi &= UINT32_C(0x0000000F); /* cap to 4 bits of extension */
#else
    t->samplecount &= UINT64_C(0x0000000FFFFFFFFF); /* cap to 36 bits */
#endif

    return 0;
}

TFLAC_PUBLIC
int tflac_encode_s16p(tflac* t, tflac_u32 blocksize, tflac_s16** samples, void* buffer, tflac_u32 len, tflac_u32* used) {
    tflac_encode_params p;

    p.blocksize = blocksize;
    p.buffer_len = len;
    p.buffer = buffer;
    p.used = used;
    p.samples = samples;
    p.calculate_md5 = (tflac_md5_calculator)tflac_update_md5_int16_planar;
    p.decorrelate = (tflac_stereo_decorrelator)tflac_stereo_decorrelate_int16_planar;

    return tflac_encode(t, &p);
}

TFLAC_PUBLIC
int tflac_encode_s16i(tflac* t, tflac_u32 blocksize, tflac_s16* samples, void* buffer, tflac_u32 len, tflac_u32* used) {
    tflac_encode_params p;

    p.blocksize = blocksize;
    p.buffer_len = len;
    p.buffer = buffer;
    p.used = used;
    p.samples = samples;
    p.decorrelate = (tflac_stereo_decorrelator)tflac_stereo_decorrelate_int16_interleaved;

    switch((7 + t->bitdepth) & 0xF8) {
        case 8:  p.calculate_md5 = (tflac_md5_calculator)tflac_update_md5_s16i_1; break;
        case 16: p.calculate_md5 = (tflac_md5_calculator)tflac_update_md5_s16i_2; break;
    }

    return tflac_encode(t, &p);
}

TFLAC_PUBLIC
int tflac_encode_s32p(tflac* t, tflac_u32 blocksize, tflac_s32** samples, void* buffer, tflac_u32 len, tflac_u32* used) {
    tflac_encode_params p;

    p.blocksize = blocksize;
    p.buffer_len = len;
    p.buffer = buffer;
    p.used = used;
    p.samples = samples;
    p.calculate_md5 = (tflac_md5_calculator)tflac_update_md5_int32_planar;
    p.decorrelate = (tflac_stereo_decorrelator)tflac_stereo_decorrelate_int32_planar;

    return tflac_encode(t, &p);
}

TFLAC_PUBLIC
int tflac_encode_s32i(tflac* t, tflac_u32 blocksize, tflac_s32* samples, void* buffer, tflac_u32 len, tflac_u32* used) {
    tflac_encode_params p;

    p.blocksize = blocksize;
    p.buffer_len = len;
    p.buffer = buffer;
    p.used = used;
    p.samples = samples;
    p.decorrelate = (tflac_stereo_decorrelator)tflac_stereo_decorrelate_int32_interleaved;

    switch((7 + t->bitdepth) & 0xF8) {
        case 8:  p.calculate_md5 = (tflac_md5_calculator)tflac_update_md5_s32i_1; break;
        case 16: p.calculate_md5 = (tflac_md5_calculator)tflac_update_md5_s32i_2; break;
        case 24: p.calculate_md5 = (tflac_md5_calculator)tflac_update_md5_s32i_3; break;
        case 32: p.calculate_md5 = (tflac_md5_calculator)tflac_update_md5_s32i_4; break;
    }

    return tflac_encode(t, &p);
}

TFLAC_PUBLIC
TFLAC_CONST
tflac_u32 tflac_size_streaminfo(void) {
    return TFLAC_SIZE_STREAMINFO;
}

TFLAC_PUBLIC
TFLAC_CONST
tflac_u32 tflac_size(void) {
    return sizeof(tflac);
}

TFLAC_PUBLIC
void tflac_finalize(tflac* t) {
    if(t->enable_md5) {
        tflac_md5_finalize(&t->md5_ctx);
        tflac_md5_digest(&t->md5_ctx, t->md5_digest);
    }
    tflac_md5_init(&t->md5_ctx);
}

TFLAC_PUBLIC
int tflac_encode_streaminfo(const tflac* t, tflac_u32 lastflag, void* buffer, tflac_u32 len, tflac_u32* used) {
    int r;
    tflac_bitwriter bw;

    tflac_bitwriter_init(&bw);

    bw.buffer = buffer;
    bw.len = len;

    if( (r = tflac_bitwriter_add(&bw, 1, lastflag)) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 7, 0)) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 24, 34)) != 0) return r;

    /* min/max block sizes */
    if( (r = tflac_bitwriter_add(&bw, 16, t->blocksize)) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 16, t->blocksize)) != 0) return r;

    /* min/max frame sizes */
    if( (r = tflac_bitwriter_add(&bw, 24, t->min_frame_size)) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 24, t->max_frame_size)) != 0) return r;

    /* sample rate */
    if( (r = tflac_bitwriter_add(&bw, 20, t->samplerate)) != 0) return r;

    /* channels -1 */
    if( (r = tflac_bitwriter_add(&bw, 3, t->channels - 1)) != 0) return r;

    /* bps - 1 */
    if( (r = tflac_bitwriter_add(&bw, 5, t->bitdepth - 1)) != 0) return r;

    /* total samples */
#ifdef TFLAC_32BIT_ONLY
    if( (r = tflac_bitwriter_add(&bw, 4, t->samplecount.hi)) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 16, t->samplecount.lo >> 16)) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 16, t->samplecount.lo )) != 0) return r;
#else
    if( (r = tflac_bitwriter_add(&bw, 36, t->samplecount)) != 0) return r;
#endif

    /* md5 checksum */
    if( (r = tflac_bitwriter_add(&bw, 8, t->md5_digest[0])) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 8, t->md5_digest[1])) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 8, t->md5_digest[2])) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 8, t->md5_digest[3])) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 8, t->md5_digest[4])) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 8, t->md5_digest[5])) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 8, t->md5_digest[6])) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 8, t->md5_digest[7])) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 8, t->md5_digest[8])) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 8, t->md5_digest[9])) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 8, t->md5_digest[10])) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 8, t->md5_digest[11])) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 8, t->md5_digest[12])) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 8, t->md5_digest[13])) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 8, t->md5_digest[14])) != 0) return r;
    if( (r = tflac_bitwriter_add(&bw, 8, t->md5_digest[15])) != 0) return r;
    if( (r = tflac_bitwriter_flush(&bw)) != 0) return r;

    *used = bw.pos;
    return 0;
}

TFLAC_PUBLIC void tflac_set_blocksize(tflac *t, tflac_u32 blocksize) {
    t->blocksize = blocksize;
}

TFLAC_PUBLIC void tflac_set_samplerate(tflac *t, tflac_u32 samplerate) {
    t->samplerate = samplerate;
}

TFLAC_PUBLIC void tflac_set_channels(tflac *t, tflac_u32 channels) {
    t->channels = channels;
}

TFLAC_PUBLIC void tflac_set_bitdepth(tflac *t, tflac_u32 bitdepth) {
    t->bitdepth = bitdepth;
}

TFLAC_PUBLIC void tflac_set_channel_mode(tflac *t, tflac_u32 channel_mode) {
    t->channel_mode = (tflac_u8)channel_mode;
}

TFLAC_PUBLIC void tflac_set_max_rice_value(tflac *t, tflac_u32 max_rice_value) {
    t->max_rice_value = (tflac_u8)max_rice_value;
}

TFLAC_PUBLIC void tflac_set_min_partition_order(tflac *t, tflac_u32 min_partition_order) {
    t->min_partition_order = (tflac_u8)min_partition_order;
}

TFLAC_PUBLIC void tflac_set_max_partition_order(tflac *t, tflac_u32 max_partition_order) {
    t->max_partition_order = (tflac_u8)max_partition_order;
}

TFLAC_PUBLIC void tflac_set_constant_subframe(tflac* t, tflac_u32 enable) {
    t->enable_constant_subframe = (tflac_u8)enable;
}

TFLAC_PUBLIC void tflac_set_fixed_subframe(tflac* t, tflac_u32 enable) {
    t->enable_fixed_subframe = (tflac_u8)enable;
}

TFLAC_PUBLIC void tflac_set_enable_md5(tflac* t, tflac_u32 enable) {
    t->enable_md5 = (tflac_u8)enable;
}

TFLAC_PUBLIC
tflac_u32 tflac_enable_sse2(tflac* t, tflac_u32 enable) {
#ifdef TFLAC_ENABLE_SSE2
    if(enable) {
        t->calculate_order[0] = tflac_cfr_order0_sse2;
        t->calculate_order[1] = tflac_cfr_order1_sse2;
        t->calculate_order[2] = tflac_cfr_order2_sse2;
        t->calculate_order[3] = tflac_cfr_order3_sse2;
        t->calculate_order[4] = tflac_cfr_order4_sse2;
    } else {
        t->calculate_order[0] = tflac_cfr_order0_std;
        t->calculate_order[1] = tflac_cfr_order1_std;
        t->calculate_order[2] = tflac_cfr_order2_std;
        t->calculate_order[3] = tflac_cfr_order3_std;
        t->calculate_order[4] = tflac_cfr_order4_std;
    }
    switch(t->bitdepth) {
        case 32: {
            t->calculate_order[1] = tflac_cfr_order1_wide;
        }
        /* fall-through */
        case 31: {
            t->calculate_order[2] = tflac_cfr_order2_wide;
        }
        /* fall-through */
        case 30: {
            t->calculate_order[3] = tflac_cfr_order3_wide;
        }
        /* fall-through */
        case 29: {
            t->calculate_order[4] = tflac_cfr_order4_wide;
        }
        /* fall-through */
        default: break;
    }
    return 0;
#else
    (void)t;
    (void)enable;
    return 1;
#endif
}

TFLAC_PUBLIC
tflac_u32 tflac_enable_ssse3(tflac* t, tflac_u32 enable) {
#ifdef TFLAC_ENABLE_SSSE3
    if(enable) {
        t->calculate_order[0] = tflac_cfr_order0_ssse3;
        t->calculate_order[1] = tflac_cfr_order1_ssse3;
        t->calculate_order[2] = tflac_cfr_order2_ssse3;
        t->calculate_order[3] = tflac_cfr_order3_ssse3;
        t->calculate_order[4] = tflac_cfr_order4_ssse3;
    } else {
        t->calculate_order[0] = tflac_cfr_order0_std;
        t->calculate_order[1] = tflac_cfr_order1_std;
        t->calculate_order[2] = tflac_cfr_order2_std;
        t->calculate_order[3] = tflac_cfr_order3_std;
        t->calculate_order[4] = tflac_cfr_order4_std;
    }
    switch(t->bitdepth) {
        case 32: {
            t->calculate_order[1] = tflac_cfr_order1_wide;
        }
        /* fall-through */
        case 31: {
            t->calculate_order[2] = tflac_cfr_order2_wide;
        }
        /* fall-through */
        case 30: {
            t->calculate_order[3] = tflac_cfr_order3_wide;
        }
        /* fall-through */
        case 29: {
            t->calculate_order[4] = tflac_cfr_order4_wide;
        }
        /* fall-through */
        default: break;
    }
    return 0;
#else
    (void)t;
    (void)enable;
    return 1;
#endif
}

TFLAC_PUBLIC
tflac_u32 tflac_enable_sse4_1(tflac* t, tflac_u32 enable) {
#ifdef TFLAC_ENABLE_SSE4_1
    if(enable) {
        t->calculate_order[0] = tflac_cfr_order0_sse4_1;
        t->calculate_order[1] = tflac_cfr_order1_sse4_1;
        t->calculate_order[2] = tflac_cfr_order2_sse4_1;
        t->calculate_order[3] = tflac_cfr_order3_sse4_1;
        t->calculate_order[4] = tflac_cfr_order4_sse4_1;
    } else {
        t->calculate_order[0] = tflac_cfr_order0_std;
        t->calculate_order[1] = tflac_cfr_order1_std;
        t->calculate_order[2] = tflac_cfr_order2_std;
        t->calculate_order[3] = tflac_cfr_order3_std;
        t->calculate_order[4] = tflac_cfr_order4_std;
    }
    switch(t->bitdepth) {
        case 32: {
            t->calculate_order[1] = tflac_cfr_order1_wide;
        }
        /* fall-through */
        case 31: {
            t->calculate_order[2] = tflac_cfr_order2_wide;
        }
        /* fall-through */
        case 30: {
            t->calculate_order[3] = tflac_cfr_order3_wide;
        }
        /* fall-through */
        case 29: {
            t->calculate_order[4] = tflac_cfr_order4_wide;
        }
        /* fall-through */
        default: break;
    }
    return 0;
#else
    (void)t;
    (void)enable;
    return 1;
#endif
}

TFLAC_PURE TFLAC_PUBLIC tflac_u32 tflac_get_enable_sse2(const tflac* t) {
#ifdef TFLAC_ENABLE_SSE2
    return t->calculate_order[0] == tflac_cfr_order0_sse2;
#else
    (void)t;
    return 0;
#endif
}

TFLAC_PURE TFLAC_PUBLIC tflac_u32 tflac_get_enable_ssse3(const tflac* t) {
#ifdef TFLAC_ENABLE_SSSE3
    return t->calculate_order[0] == tflac_cfr_order0_ssse3;
#else
    (void)t;
    return 0;
#endif
}

TFLAC_PURE TFLAC_PUBLIC tflac_u32 tflac_get_enable_sse4_1(const tflac* t) {
#ifdef TFLAC_ENABLE_SSE4_1
    return t->calculate_order[0] == tflac_cfr_order0_sse4_1;
#else
    (void)t;
    return 0;
#endif
}

TFLAC_PURE TFLAC_PUBLIC tflac_u32 tflac_get_blocksize(const tflac* t) {
    return t->blocksize;
}

TFLAC_PURE TFLAC_PUBLIC tflac_u32 tflac_get_samplerate(const tflac* t) {
    return t->samplerate;
}

TFLAC_PURE TFLAC_PUBLIC tflac_u32 tflac_get_channels(const tflac* t) {
    return t->channels;
}

TFLAC_PURE TFLAC_PUBLIC tflac_u32 tflac_get_bitdepth(const tflac* t) {
    return t->bitdepth;
}

TFLAC_PURE TFLAC_PUBLIC tflac_u32 tflac_get_channel_mode(const tflac* t) {
    return t->channel_mode >= ((tflac_u32) TFLAC_CHANNEL_MODE_COUNT) ? 0 : t->channel_mode;
}

TFLAC_PURE TFLAC_PUBLIC tflac_u32 tflac_get_max_rice_value(const tflac* t) {
    return t->max_rice_value;
}

TFLAC_PURE TFLAC_PUBLIC tflac_u32 tflac_get_min_partition_order(const tflac* t) {
    return t->min_partition_order;
}

TFLAC_PURE TFLAC_PUBLIC tflac_u32 tflac_get_max_partition_order(const tflac* t) {
    return t->max_partition_order;
}

TFLAC_PURE TFLAC_PUBLIC tflac_u32 tflac_get_constant_subframe(const tflac* t) {
    return t->enable_constant_subframe;
}

TFLAC_PURE TFLAC_PUBLIC tflac_u32 tflac_get_fixed_subframe(const tflac* t) {
    return t->enable_fixed_subframe;
}

TFLAC_PURE TFLAC_PUBLIC tflac_u32 tflac_get_wasted_bits(const tflac* t) {
    return t->wasted_bits;
}

TFLAC_PURE TFLAC_PUBLIC tflac_u32 tflac_get_constant(const tflac* t) {
    return t->constant;
}

TFLAC_PURE TFLAC_PUBLIC tflac_u32 tflac_get_enable_md5(const tflac* t) {
    return t->enable_md5;
}

/* TODO:
 *
 *   For SUBFRAME_FIXED:
 *
 *       I'm not sure if the best thing to is is pick the largest partition
 *       order since that results in the smallest partition size, or to try
 *       all partition orders until the one that results in the smallest
 *       length is found?
 *
 *       Using the largest order seems to be seems to be really, really fast
 *       and still get decent compression results.
 */

TFLAC_PRIVATE const tflac_u8 tflac_crc8_table[256] = {
  0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15,
  0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d,
  0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65,
  0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d,
  0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5,
  0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd,
  0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85,
  0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd,
  0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2,
  0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea,
  0xb7, 0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2,
  0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a,
  0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32,
  0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a,
  0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42,
  0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a,
  0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c,
  0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
  0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec,
  0xc1, 0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4,
  0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c,
  0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44,
  0x19, 0x1e, 0x17, 0x10, 0x05, 0x02, 0x0b, 0x0c,
  0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34,
  0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b,
  0x76, 0x71, 0x78, 0x7f, 0x6a, 0x6d, 0x64, 0x63,
  0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b,
  0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13,
  0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb,
  0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8d, 0x84, 0x83,
  0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb,
  0xe6, 0xe1, 0xe8, 0xef, 0xfa, 0xfd, 0xf4, 0xf3,
};

TFLAC_PRIVATE const tflac_u16 tflac_crc16_tables[8][256] = {
  {
    0x0000,  0x8005,  0x800f,  0x000a,  0x801b,  0x001e,  0x0014,  0x8011,
    0x8033,  0x0036,  0x003c,  0x8039,  0x0028,  0x802d,  0x8027,  0x0022,
    0x8063,  0x0066,  0x006c,  0x8069,  0x0078,  0x807d,  0x8077,  0x0072,
    0x0050,  0x8055,  0x805f,  0x005a,  0x804b,  0x004e,  0x0044,  0x8041,
    0x80c3,  0x00c6,  0x00cc,  0x80c9,  0x00d8,  0x80dd,  0x80d7,  0x00d2,
    0x00f0,  0x80f5,  0x80ff,  0x00fa,  0x80eb,  0x00ee,  0x00e4,  0x80e1,
    0x00a0,  0x80a5,  0x80af,  0x00aa,  0x80bb,  0x00be,  0x00b4,  0x80b1,
    0x8093,  0x0096,  0x009c,  0x8099,  0x0088,  0x808d,  0x8087,  0x0082,
    0x8183,  0x0186,  0x018c,  0x8189,  0x0198,  0x819d,  0x8197,  0x0192,
    0x01b0,  0x81b5,  0x81bf,  0x01ba,  0x81ab,  0x01ae,  0x01a4,  0x81a1,
    0x01e0,  0x81e5,  0x81ef,  0x01ea,  0x81fb,  0x01fe,  0x01f4,  0x81f1,
    0x81d3,  0x01d6,  0x01dc,  0x81d9,  0x01c8,  0x81cd,  0x81c7,  0x01c2,
    0x0140,  0x8145,  0x814f,  0x014a,  0x815b,  0x015e,  0x0154,  0x8151,
    0x8173,  0x0176,  0x017c,  0x8179,  0x0168,  0x816d,  0x8167,  0x0162,
    0x8123,  0x0126,  0x012c,  0x8129,  0x0138,  0x813d,  0x8137,  0x0132,
    0x0110,  0x8115,  0x811f,  0x011a,  0x810b,  0x010e,  0x0104,  0x8101,
    0x8303,  0x0306,  0x030c,  0x8309,  0x0318,  0x831d,  0x8317,  0x0312,
    0x0330,  0x8335,  0x833f,  0x033a,  0x832b,  0x032e,  0x0324,  0x8321,
    0x0360,  0x8365,  0x836f,  0x036a,  0x837b,  0x037e,  0x0374,  0x8371,
    0x8353,  0x0356,  0x035c,  0x8359,  0x0348,  0x834d,  0x8347,  0x0342,
    0x03c0,  0x83c5,  0x83cf,  0x03ca,  0x83db,  0x03de,  0x03d4,  0x83d1,
    0x83f3,  0x03f6,  0x03fc,  0x83f9,  0x03e8,  0x83ed,  0x83e7,  0x03e2,
    0x83a3,  0x03a6,  0x03ac,  0x83a9,  0x03b8,  0x83bd,  0x83b7,  0x03b2,
    0x0390,  0x8395,  0x839f,  0x039a,  0x838b,  0x038e,  0x0384,  0x8381,
    0x0280,  0x8285,  0x828f,  0x028a,  0x829b,  0x029e,  0x0294,  0x8291,
    0x82b3,  0x02b6,  0x02bc,  0x82b9,  0x02a8,  0x82ad,  0x82a7,  0x02a2,
    0x82e3,  0x02e6,  0x02ec,  0x82e9,  0x02f8,  0x82fd,  0x82f7,  0x02f2,
    0x02d0,  0x82d5,  0x82df,  0x02da,  0x82cb,  0x02ce,  0x02c4,  0x82c1,
    0x8243,  0x0246,  0x024c,  0x8249,  0x0258,  0x825d,  0x8257,  0x0252,
    0x0270,  0x8275,  0x827f,  0x027a,  0x826b,  0x026e,  0x0264,  0x8261,
    0x0220,  0x8225,  0x822f,  0x022a,  0x823b,  0x023e,  0x0234,  0x8231,
    0x8213,  0x0216,  0x021c,  0x8219,  0x0208,  0x820d,  0x8207,  0x0202,
  },
  {
    0x0000,  0x8603,  0x8c03,  0x0a00,  0x9803,  0x1e00,  0x1400,  0x9203,
    0xb003,  0x3600,  0x3c00,  0xba03,  0x2800,  0xae03,  0xa403,  0x2200,
    0xe003,  0x6600,  0x6c00,  0xea03,  0x7800,  0xfe03,  0xf403,  0x7200,
    0x5000,  0xd603,  0xdc03,  0x5a00,  0xc803,  0x4e00,  0x4400,  0xc203,
    0x4003,  0xc600,  0xcc00,  0x4a03,  0xd800,  0x5e03,  0x5403,  0xd200,
    0xf000,  0x7603,  0x7c03,  0xfa00,  0x6803,  0xee00,  0xe400,  0x6203,
    0xa000,  0x2603,  0x2c03,  0xaa00,  0x3803,  0xbe00,  0xb400,  0x3203,
    0x1003,  0x9600,  0x9c00,  0x1a03,  0x8800,  0x0e03,  0x0403,  0x8200,
    0x8006,  0x0605,  0x0c05,  0x8a06,  0x1805,  0x9e06,  0x9406,  0x1205,
    0x3005,  0xb606,  0xbc06,  0x3a05,  0xa806,  0x2e05,  0x2405,  0xa206,
    0x6005,  0xe606,  0xec06,  0x6a05,  0xf806,  0x7e05,  0x7405,  0xf206,
    0xd006,  0x5605,  0x5c05,  0xda06,  0x4805,  0xce06,  0xc406,  0x4205,
    0xc005,  0x4606,  0x4c06,  0xca05,  0x5806,  0xde05,  0xd405,  0x5206,
    0x7006,  0xf605,  0xfc05,  0x7a06,  0xe805,  0x6e06,  0x6406,  0xe205,
    0x2006,  0xa605,  0xac05,  0x2a06,  0xb805,  0x3e06,  0x3406,  0xb205,
    0x9005,  0x1606,  0x1c06,  0x9a05,  0x0806,  0x8e05,  0x8405,  0x0206,
    0x8009,  0x060a,  0x0c0a,  0x8a09,  0x180a,  0x9e09,  0x9409,  0x120a,
    0x300a,  0xb609,  0xbc09,  0x3a0a,  0xa809,  0x2e0a,  0x240a,  0xa209,
    0x600a,  0xe609,  0xec09,  0x6a0a,  0xf809,  0x7e0a,  0x740a,  0xf209,
    0xd009,  0x560a,  0x5c0a,  0xda09,  0x480a,  0xce09,  0xc409,  0x420a,
    0xc00a,  0x4609,  0x4c09,  0xca0a,  0x5809,  0xde0a,  0xd40a,  0x5209,
    0x7009,  0xf60a,  0xfc0a,  0x7a09,  0xe80a,  0x6e09,  0x6409,  0xe20a,
    0x2009,  0xa60a,  0xac0a,  0x2a09,  0xb80a,  0x3e09,  0x3409,  0xb20a,
    0x900a,  0x1609,  0x1c09,  0x9a0a,  0x0809,  0x8e0a,  0x840a,  0x0209,
    0x000f,  0x860c,  0x8c0c,  0x0a0f,  0x980c,  0x1e0f,  0x140f,  0x920c,
    0xb00c,  0x360f,  0x3c0f,  0xba0c,  0x280f,  0xae0c,  0xa40c,  0x220f,
    0xe00c,  0x660f,  0x6c0f,  0xea0c,  0x780f,  0xfe0c,  0xf40c,  0x720f,
    0x500f,  0xd60c,  0xdc0c,  0x5a0f,  0xc80c,  0x4e0f,  0x440f,  0xc20c,
    0x400c,  0xc60f,  0xcc0f,  0x4a0c,  0xd80f,  0x5e0c,  0x540c,  0xd20f,
    0xf00f,  0x760c,  0x7c0c,  0xfa0f,  0x680c,  0xee0f,  0xe40f,  0x620c,
    0xa00f,  0x260c,  0x2c0c,  0xaa0f,  0x380c,  0xbe0f,  0xb40f,  0x320c,
    0x100c,  0x960f,  0x9c0f,  0x1a0c,  0x880f,  0x0e0c,  0x040c,  0x820f,
  },
  {
    0x0000,  0x8017,  0x802b,  0x003c,  0x8053,  0x0044,  0x0078,  0x806f,
    0x80a3,  0x00b4,  0x0088,  0x809f,  0x00f0,  0x80e7,  0x80db,  0x00cc,
    0x8143,  0x0154,  0x0168,  0x817f,  0x0110,  0x8107,  0x813b,  0x012c,
    0x01e0,  0x81f7,  0x81cb,  0x01dc,  0x81b3,  0x01a4,  0x0198,  0x818f,
    0x8283,  0x0294,  0x02a8,  0x82bf,  0x02d0,  0x82c7,  0x82fb,  0x02ec,
    0x0220,  0x8237,  0x820b,  0x021c,  0x8273,  0x0264,  0x0258,  0x824f,
    0x03c0,  0x83d7,  0x83eb,  0x03fc,  0x8393,  0x0384,  0x03b8,  0x83af,
    0x8363,  0x0374,  0x0348,  0x835f,  0x0330,  0x8327,  0x831b,  0x030c,
    0x8503,  0x0514,  0x0528,  0x853f,  0x0550,  0x8547,  0x857b,  0x056c,
    0x05a0,  0x85b7,  0x858b,  0x059c,  0x85f3,  0x05e4,  0x05d8,  0x85cf,
    0x0440,  0x8457,  0x846b,  0x047c,  0x8413,  0x0404,  0x0438,  0x842f,
    0x84e3,  0x04f4,  0x04c8,  0x84df,  0x04b0,  0x84a7,  0x849b,  0x048c,
    0x0780,  0x8797,  0x87ab,  0x07bc,  0x87d3,  0x07c4,  0x07f8,  0x87ef,
    0x8723,  0x0734,  0x0708,  0x871f,  0x0770,  0x8767,  0x875b,  0x074c,
    0x86c3,  0x06d4,  0x06e8,  0x86ff,  0x0690,  0x8687,  0x86bb,  0x06ac,
    0x0660,  0x8677,  0x864b,  0x065c,  0x8633,  0x0624,  0x0618,  0x860f,
    0x8a03,  0x0a14,  0x0a28,  0x8a3f,  0x0a50,  0x8a47,  0x8a7b,  0x0a6c,
    0x0aa0,  0x8ab7,  0x8a8b,  0x0a9c,  0x8af3,  0x0ae4,  0x0ad8,  0x8acf,
    0x0b40,  0x8b57,  0x8b6b,  0x0b7c,  0x8b13,  0x0b04,  0x0b38,  0x8b2f,
    0x8be3,  0x0bf4,  0x0bc8,  0x8bdf,  0x0bb0,  0x8ba7,  0x8b9b,  0x0b8c,
    0x0880,  0x8897,  0x88ab,  0x08bc,  0x88d3,  0x08c4,  0x08f8,  0x88ef,
    0x8823,  0x0834,  0x0808,  0x881f,  0x0870,  0x8867,  0x885b,  0x084c,
    0x89c3,  0x09d4,  0x09e8,  0x89ff,  0x0990,  0x8987,  0x89bb,  0x09ac,
    0x0960,  0x8977,  0x894b,  0x095c,  0x8933,  0x0924,  0x0918,  0x890f,
    0x0f00,  0x8f17,  0x8f2b,  0x0f3c,  0x8f53,  0x0f44,  0x0f78,  0x8f6f,
    0x8fa3,  0x0fb4,  0x0f88,  0x8f9f,  0x0ff0,  0x8fe7,  0x8fdb,  0x0fcc,
    0x8e43,  0x0e54,  0x0e68,  0x8e7f,  0x0e10,  0x8e07,  0x8e3b,  0x0e2c,
    0x0ee0,  0x8ef7,  0x8ecb,  0x0edc,  0x8eb3,  0x0ea4,  0x0e98,  0x8e8f,
    0x8d83,  0x0d94,  0x0da8,  0x8dbf,  0x0dd0,  0x8dc7,  0x8dfb,  0x0dec,
    0x0d20,  0x8d37,  0x8d0b,  0x0d1c,  0x8d73,  0x0d64,  0x0d58,  0x8d4f,
    0x0cc0,  0x8cd7,  0x8ceb,  0x0cfc,  0x8c93,  0x0c84,  0x0cb8,  0x8caf,
    0x8c63,  0x0c74,  0x0c48,  0x8c5f,  0x0c30,  0x8c27,  0x8c1b,  0x0c0c,
  },
  {
    0x0000,  0x9403,  0xa803,  0x3c00,  0xd003,  0x4400,  0x7800,  0xec03,
    0x2003,  0xb400,  0x8800,  0x1c03,  0xf000,  0x6403,  0x5803,  0xcc00,
    0x4006,  0xd405,  0xe805,  0x7c06,  0x9005,  0x0406,  0x3806,  0xac05,
    0x6005,  0xf406,  0xc806,  0x5c05,  0xb006,  0x2405,  0x1805,  0x8c06,
    0x800c,  0x140f,  0x280f,  0xbc0c,  0x500f,  0xc40c,  0xf80c,  0x6c0f,
    0xa00f,  0x340c,  0x080c,  0x9c0f,  0x700c,  0xe40f,  0xd80f,  0x4c0c,
    0xc00a,  0x5409,  0x6809,  0xfc0a,  0x1009,  0x840a,  0xb80a,  0x2c09,
    0xe009,  0x740a,  0x480a,  0xdc09,  0x300a,  0xa409,  0x9809,  0x0c0a,
    0x801d,  0x141e,  0x281e,  0xbc1d,  0x501e,  0xc41d,  0xf81d,  0x6c1e,
    0xa01e,  0x341d,  0x081d,  0x9c1e,  0x701d,  0xe41e,  0xd81e,  0x4c1d,
    0xc01b,  0x5418,  0x6818,  0xfc1b,  0x1018,  0x841b,  0xb81b,  0x2c18,
    0xe018,  0x741b,  0x481b,  0xdc18,  0x301b,  0xa418,  0x9818,  0x0c1b,
    0x0011,  0x9412,  0xa812,  0x3c11,  0xd012,  0x4411,  0x7811,  0xec12,
    0x2012,  0xb411,  0x8811,  0x1c12,  0xf011,  0x6412,  0x5812,  0xcc11,
    0x4017,  0xd414,  0xe814,  0x7c17,  0x9014,  0x0417,  0x3817,  0xac14,
    0x6014,  0xf417,  0xc817,  0x5c14,  0xb017,  0x2414,  0x1814,  0x8c17,
    0x803f,  0x143c,  0x283c,  0xbc3f,  0x503c,  0xc43f,  0xf83f,  0x6c3c,
    0xa03c,  0x343f,  0x083f,  0x9c3c,  0x703f,  0xe43c,  0xd83c,  0x4c3f,
    0xc039,  0x543a,  0x683a,  0xfc39,  0x103a,  0x8439,  0xb839,  0x2c3a,
    0xe03a,  0x7439,  0x4839,  0xdc3a,  0x3039,  0xa43a,  0x983a,  0x0c39,
    0x0033,  0x9430,  0xa830,  0x3c33,  0xd030,  0x4433,  0x7833,  0xec30,
    0x2030,  0xb433,  0x8833,  0x1c30,  0xf033,  0x6430,  0x5830,  0xcc33,
    0x4035,  0xd436,  0xe836,  0x7c35,  0x9036,  0x0435,  0x3835,  0xac36,
    0x6036,  0xf435,  0xc835,  0x5c36,  0xb035,  0x2436,  0x1836,  0x8c35,
    0x0022,  0x9421,  0xa821,  0x3c22,  0xd021,  0x4422,  0x7822,  0xec21,
    0x2021,  0xb422,  0x8822,  0x1c21,  0xf022,  0x6421,  0x5821,  0xcc22,
    0x4024,  0xd427,  0xe827,  0x7c24,  0x9027,  0x0424,  0x3824,  0xac27,
    0x6027,  0xf424,  0xc824,  0x5c27,  0xb024,  0x2427,  0x1827,  0x8c24,
    0x802e,  0x142d,  0x282d,  0xbc2e,  0x502d,  0xc42e,  0xf82e,  0x6c2d,
    0xa02d,  0x342e,  0x082e,  0x9c2d,  0x702e,  0xe42d,  0xd82d,  0x4c2e,
    0xc028,  0x542b,  0x682b,  0xfc28,  0x102b,  0x8428,  0xb828,  0x2c2b,
    0xe02b,  0x7428,  0x4828,  0xdc2b,  0x3028,  0xa42b,  0x982b,  0x0c28,
  },
  {
    0x0000,  0x807b,  0x80f3,  0x0088,  0x81e3,  0x0198,  0x0110,  0x816b,
    0x83c3,  0x03b8,  0x0330,  0x834b,  0x0220,  0x825b,  0x82d3,  0x02a8,
    0x8783,  0x07f8,  0x0770,  0x870b,  0x0660,  0x861b,  0x8693,  0x06e8,
    0x0440,  0x843b,  0x84b3,  0x04c8,  0x85a3,  0x05d8,  0x0550,  0x852b,
    0x8f03,  0x0f78,  0x0ff0,  0x8f8b,  0x0ee0,  0x8e9b,  0x8e13,  0x0e68,
    0x0cc0,  0x8cbb,  0x8c33,  0x0c48,  0x8d23,  0x0d58,  0x0dd0,  0x8dab,
    0x0880,  0x88fb,  0x8873,  0x0808,  0x8963,  0x0918,  0x0990,  0x89eb,
    0x8b43,  0x0b38,  0x0bb0,  0x8bcb,  0x0aa0,  0x8adb,  0x8a53,  0x0a28,
    0x9e03,  0x1e78,  0x1ef0,  0x9e8b,  0x1fe0,  0x9f9b,  0x9f13,  0x1f68,
    0x1dc0,  0x9dbb,  0x9d33,  0x1d48,  0x9c23,  0x1c58,  0x1cd0,  0x9cab,
    0x1980,  0x99fb,  0x9973,  0x1908,  0x9863,  0x1818,  0x1890,  0x98eb,
    0x9a43,  0x1a38,  0x1ab0,  0x9acb,  0x1ba0,  0x9bdb,  0x9b53,  0x1b28,
    0x1100,  0x917b,  0x91f3,  0x1188,  0x90e3,  0x1098,  0x1010,  0x906b,
    0x92c3,  0x12b8,  0x1230,  0x924b,  0x1320,  0x935b,  0x93d3,  0x13a8,
    0x9683,  0x16f8,  0x1670,  0x960b,  0x1760,  0x971b,  0x9793,  0x17e8,
    0x1540,  0x953b,  0x95b3,  0x15c8,  0x94a3,  0x14d8,  0x1450,  0x942b,
    0xbc03,  0x3c78,  0x3cf0,  0xbc8b,  0x3de0,  0xbd9b,  0xbd13,  0x3d68,
    0x3fc0,  0xbfbb,  0xbf33,  0x3f48,  0xbe23,  0x3e58,  0x3ed0,  0xbeab,
    0x3b80,  0xbbfb,  0xbb73,  0x3b08,  0xba63,  0x3a18,  0x3a90,  0xbaeb,
    0xb843,  0x3838,  0x38b0,  0xb8cb,  0x39a0,  0xb9db,  0xb953,  0x3928,
    0x3300,  0xb37b,  0xb3f3,  0x3388,  0xb2e3,  0x3298,  0x3210,  0xb26b,
    0xb0c3,  0x30b8,  0x3030,  0xb04b,  0x3120,  0xb15b,  0xb1d3,  0x31a8,
    0xb483,  0x34f8,  0x3470,  0xb40b,  0x3560,  0xb51b,  0xb593,  0x35e8,
    0x3740,  0xb73b,  0xb7b3,  0x37c8,  0xb6a3,  0x36d8,  0x3650,  0xb62b,
    0x2200,  0xa27b,  0xa2f3,  0x2288,  0xa3e3,  0x2398,  0x2310,  0xa36b,
    0xa1c3,  0x21b8,  0x2130,  0xa14b,  0x2020,  0xa05b,  0xa0d3,  0x20a8,
    0xa583,  0x25f8,  0x2570,  0xa50b,  0x2460,  0xa41b,  0xa493,  0x24e8,
    0x2640,  0xa63b,  0xa6b3,  0x26c8,  0xa7a3,  0x27d8,  0x2750,  0xa72b,
    0xad03,  0x2d78,  0x2df0,  0xad8b,  0x2ce0,  0xac9b,  0xac13,  0x2c68,
    0x2ec0,  0xaebb,  0xae33,  0x2e48,  0xaf23,  0x2f58,  0x2fd0,  0xafab,
    0x2a80,  0xaafb,  0xaa73,  0x2a08,  0xab63,  0x2b18,  0x2b90,  0xabeb,
    0xa943,  0x2938,  0x29b0,  0xa9cb,  0x28a0,  0xa8db,  0xa853,  0x2828,
  },
  {
    0x0000,  0xf803,  0x7003,  0x8800,  0xe006,  0x1805,  0x9005,  0x6806,
    0x4009,  0xb80a,  0x300a,  0xc809,  0xa00f,  0x580c,  0xd00c,  0x280f,
    0x8012,  0x7811,  0xf011,  0x0812,  0x6014,  0x9817,  0x1017,  0xe814,
    0xc01b,  0x3818,  0xb018,  0x481b,  0x201d,  0xd81e,  0x501e,  0xa81d,
    0x8021,  0x7822,  0xf022,  0x0821,  0x6027,  0x9824,  0x1024,  0xe827,
    0xc028,  0x382b,  0xb02b,  0x4828,  0x202e,  0xd82d,  0x502d,  0xa82e,
    0x0033,  0xf830,  0x7030,  0x8833,  0xe035,  0x1836,  0x9036,  0x6835,
    0x403a,  0xb839,  0x3039,  0xc83a,  0xa03c,  0x583f,  0xd03f,  0x283c,
    0x8047,  0x7844,  0xf044,  0x0847,  0x6041,  0x9842,  0x1042,  0xe841,
    0xc04e,  0x384d,  0xb04d,  0x484e,  0x2048,  0xd84b,  0x504b,  0xa848,
    0x0055,  0xf856,  0x7056,  0x8855,  0xe053,  0x1850,  0x9050,  0x6853,
    0x405c,  0xb85f,  0x305f,  0xc85c,  0xa05a,  0x5859,  0xd059,  0x285a,
    0x0066,  0xf865,  0x7065,  0x8866,  0xe060,  0x1863,  0x9063,  0x6860,
    0x406f,  0xb86c,  0x306c,  0xc86f,  0xa069,  0x586a,  0xd06a,  0x2869,
    0x8074,  0x7877,  0xf077,  0x0874,  0x6072,  0x9871,  0x1071,  0xe872,
    0xc07d,  0x387e,  0xb07e,  0x487d,  0x207b,  0xd878,  0x5078,  0xa87b,
    0x808b,  0x7888,  0xf088,  0x088b,  0x608d,  0x988e,  0x108e,  0xe88d,
    0xc082,  0x3881,  0xb081,  0x4882,  0x2084,  0xd887,  0x5087,  0xa884,
    0x0099,  0xf89a,  0x709a,  0x8899,  0xe09f,  0x189c,  0x909c,  0x689f,
    0x4090,  0xb893,  0x3093,  0xc890,  0xa096,  0x5895,  0xd095,  0x2896,
    0x00aa,  0xf8a9,  0x70a9,  0x88aa,  0xe0ac,  0x18af,  0x90af,  0x68ac,
    0x40a3,  0xb8a0,  0x30a0,  0xc8a3,  0xa0a5,  0x58a6,  0xd0a6,  0x28a5,
    0x80b8,  0x78bb,  0xf0bb,  0x08b8,  0x60be,  0x98bd,  0x10bd,  0xe8be,
    0xc0b1,  0x38b2,  0xb0b2,  0x48b1,  0x20b7,  0xd8b4,  0x50b4,  0xa8b7,
    0x00cc,  0xf8cf,  0x70cf,  0x88cc,  0xe0ca,  0x18c9,  0x90c9,  0x68ca,
    0x40c5,  0xb8c6,  0x30c6,  0xc8c5,  0xa0c3,  0x58c0,  0xd0c0,  0x28c3,
    0x80de,  0x78dd,  0xf0dd,  0x08de,  0x60d8,  0x98db,  0x10db,  0xe8d8,
    0xc0d7,  0x38d4,  0xb0d4,  0x48d7,  0x20d1,  0xd8d2,  0x50d2,  0xa8d1,
    0x80ed,  0x78ee,  0xf0ee,  0x08ed,  0x60eb,  0x98e8,  0x10e8,  0xe8eb,
    0xc0e4,  0x38e7,  0xb0e7,  0x48e4,  0x20e2,  0xd8e1,  0x50e1,  0xa8e2,
    0x00ff,  0xf8fc,  0x70fc,  0x88ff,  0xe0f9,  0x18fa,  0x90fa,  0x68f9,
    0x40f6,  0xb8f5,  0x30f5,  0xc8f6,  0xa0f0,  0x58f3,  0xd0f3,  0x28f0,
  },
  {
    0x0000,  0x8113,  0x8223,  0x0330,  0x8443,  0x0550,  0x0660,  0x8773,
    0x8883,  0x0990,  0x0aa0,  0x8bb3,  0x0cc0,  0x8dd3,  0x8ee3,  0x0ff0,
    0x9103,  0x1010,  0x1320,  0x9233,  0x1540,  0x9453,  0x9763,  0x1670,
    0x1980,  0x9893,  0x9ba3,  0x1ab0,  0x9dc3,  0x1cd0,  0x1fe0,  0x9ef3,
    0xa203,  0x2310,  0x2020,  0xa133,  0x2640,  0xa753,  0xa463,  0x2570,
    0x2a80,  0xab93,  0xa8a3,  0x29b0,  0xaec3,  0x2fd0,  0x2ce0,  0xadf3,
    0x3300,  0xb213,  0xb123,  0x3030,  0xb743,  0x3650,  0x3560,  0xb473,
    0xbb83,  0x3a90,  0x39a0,  0xb8b3,  0x3fc0,  0xbed3,  0xbde3,  0x3cf0,
    0xc403,  0x4510,  0x4620,  0xc733,  0x4040,  0xc153,  0xc263,  0x4370,
    0x4c80,  0xcd93,  0xcea3,  0x4fb0,  0xc8c3,  0x49d0,  0x4ae0,  0xcbf3,
    0x5500,  0xd413,  0xd723,  0x5630,  0xd143,  0x5050,  0x5360,  0xd273,
    0xdd83,  0x5c90,  0x5fa0,  0xdeb3,  0x59c0,  0xd8d3,  0xdbe3,  0x5af0,
    0x6600,  0xe713,  0xe423,  0x6530,  0xe243,  0x6350,  0x6060,  0xe173,
    0xee83,  0x6f90,  0x6ca0,  0xedb3,  0x6ac0,  0xebd3,  0xe8e3,  0x69f0,
    0xf703,  0x7610,  0x7520,  0xf433,  0x7340,  0xf253,  0xf163,  0x7070,
    0x7f80,  0xfe93,  0xfda3,  0x7cb0,  0xfbc3,  0x7ad0,  0x79e0,  0xf8f3,
    0x0803,  0x8910,  0x8a20,  0x0b33,  0x8c40,  0x0d53,  0x0e63,  0x8f70,
    0x8080,  0x0193,  0x02a3,  0x83b0,  0x04c3,  0x85d0,  0x86e0,  0x07f3,
    0x9900,  0x1813,  0x1b23,  0x9a30,  0x1d43,  0x9c50,  0x9f60,  0x1e73,
    0x1183,  0x9090,  0x93a0,  0x12b3,  0x95c0,  0x14d3,  0x17e3,  0x96f0,
    0xaa00,  0x2b13,  0x2823,  0xa930,  0x2e43,  0xaf50,  0xac60,  0x2d73,
    0x2283,  0xa390,  0xa0a0,  0x21b3,  0xa6c0,  0x27d3,  0x24e3,  0xa5f0,
    0x3b03,  0xba10,  0xb920,  0x3833,  0xbf40,  0x3e53,  0x3d63,  0xbc70,
    0xb380,  0x3293,  0x31a3,  0xb0b0,  0x37c3,  0xb6d0,  0xb5e0,  0x34f3,
    0xcc00,  0x4d13,  0x4e23,  0xcf30,  0x4843,  0xc950,  0xca60,  0x4b73,
    0x4483,  0xc590,  0xc6a0,  0x47b3,  0xc0c0,  0x41d3,  0x42e3,  0xc3f0,
    0x5d03,  0xdc10,  0xdf20,  0x5e33,  0xd940,  0x5853,  0x5b63,  0xda70,
    0xd580,  0x5493,  0x57a3,  0xd6b0,  0x51c3,  0xd0d0,  0xd3e0,  0x52f3,
    0x6e03,  0xef10,  0xec20,  0x6d33,  0xea40,  0x6b53,  0x6863,  0xe970,
    0xe680,  0x6793,  0x64a3,  0xe5b0,  0x62c3,  0xe3d0,  0xe0e0,  0x61f3,
    0xff00,  0x7e13,  0x7d23,  0xfc30,  0x7b43,  0xfa50,  0xf960,  0x7873,
    0x7783,  0xf690,  0xf5a0,  0x74b3,  0xf3c0,  0x72d3,  0x71e3,  0xf0f0,
  },
  {
    0x0000,  0x1006,  0x200c,  0x300a,  0x4018,  0x501e,  0x6014,  0x7012,
    0x8030,  0x9036,  0xa03c,  0xb03a,  0xc028,  0xd02e,  0xe024,  0xf022,
    0x8065,  0x9063,  0xa069,  0xb06f,  0xc07d,  0xd07b,  0xe071,  0xf077,
    0x0055,  0x1053,  0x2059,  0x305f,  0x404d,  0x504b,  0x6041,  0x7047,
    0x80cf,  0x90c9,  0xa0c3,  0xb0c5,  0xc0d7,  0xd0d1,  0xe0db,  0xf0dd,
    0x00ff,  0x10f9,  0x20f3,  0x30f5,  0x40e7,  0x50e1,  0x60eb,  0x70ed,
    0x00aa,  0x10ac,  0x20a6,  0x30a0,  0x40b2,  0x50b4,  0x60be,  0x70b8,
    0x809a,  0x909c,  0xa096,  0xb090,  0xc082,  0xd084,  0xe08e,  0xf088,
    0x819b,  0x919d,  0xa197,  0xb191,  0xc183,  0xd185,  0xe18f,  0xf189,
    0x01ab,  0x11ad,  0x21a7,  0x31a1,  0x41b3,  0x51b5,  0x61bf,  0x71b9,
    0x01fe,  0x11f8,  0x21f2,  0x31f4,  0x41e6,  0x51e0,  0x61ea,  0x71ec,
    0x81ce,  0x91c8,  0xa1c2,  0xb1c4,  0xc1d6,  0xd1d0,  0xe1da,  0xf1dc,
    0x0154,  0x1152,  0x2158,  0x315e,  0x414c,  0x514a,  0x6140,  0x7146,
    0x8164,  0x9162,  0xa168,  0xb16e,  0xc17c,  0xd17a,  0xe170,  0xf176,
    0x8131,  0x9137,  0xa13d,  0xb13b,  0xc129,  0xd12f,  0xe125,  0xf123,
    0x0101,  0x1107,  0x210d,  0x310b,  0x4119,  0x511f,  0x6115,  0x7113,
    0x8333,  0x9335,  0xa33f,  0xb339,  0xc32b,  0xd32d,  0xe327,  0xf321,
    0x0303,  0x1305,  0x230f,  0x3309,  0x431b,  0x531d,  0x6317,  0x7311,
    0x0356,  0x1350,  0x235a,  0x335c,  0x434e,  0x5348,  0x6342,  0x7344,
    0x8366,  0x9360,  0xa36a,  0xb36c,  0xc37e,  0xd378,  0xe372,  0xf374,
    0x03fc,  0x13fa,  0x23f0,  0x33f6,  0x43e4,  0x53e2,  0x63e8,  0x73ee,
    0x83cc,  0x93ca,  0xa3c0,  0xb3c6,  0xc3d4,  0xd3d2,  0xe3d8,  0xf3de,
    0x8399,  0x939f,  0xa395,  0xb393,  0xc381,  0xd387,  0xe38d,  0xf38b,
    0x03a9,  0x13af,  0x23a5,  0x33a3,  0x43b1,  0x53b7,  0x63bd,  0x73bb,
    0x02a8,  0x12ae,  0x22a4,  0x32a2,  0x42b0,  0x52b6,  0x62bc,  0x72ba,
    0x8298,  0x929e,  0xa294,  0xb292,  0xc280,  0xd286,  0xe28c,  0xf28a,
    0x82cd,  0x92cb,  0xa2c1,  0xb2c7,  0xc2d5,  0xd2d3,  0xe2d9,  0xf2df,
    0x02fd,  0x12fb,  0x22f1,  0x32f7,  0x42e5,  0x52e3,  0x62e9,  0x72ef,
    0x8267,  0x9261,  0xa26b,  0xb26d,  0xc27f,  0xd279,  0xe273,  0xf275,
    0x0257,  0x1251,  0x225b,  0x325d,  0x424f,  0x5249,  0x6243,  0x7245,
    0x0202,  0x1204,  0x220e,  0x3208,  0x421a,  0x521c,  0x6216,  0x7210,
    0x8232,  0x9234,  0xa23e,  0xb238,  0xc22a,  0xd22c,  0xe226,  0xf220,
  },
};

TFLAC_PRIVATE void (*tflac_cfr_order0)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT,
    tflac_s32* TFLAC_RESTRICT,
    tflac_u64* TFLAC_RESTRICT) = tflac_cfr_order0_std;

TFLAC_PRIVATE void (*tflac_cfr_order1)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT,
    tflac_s32* TFLAC_RESTRICT,
    tflac_u64* TFLAC_RESTRICT) = tflac_cfr_order1_std;

TFLAC_PRIVATE void (*tflac_cfr_order2)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT,
    tflac_s32* TFLAC_RESTRICT,
    tflac_u64* TFLAC_RESTRICT) = tflac_cfr_order2_std;

TFLAC_PRIVATE void (*tflac_cfr_order3)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT,
    tflac_s32* TFLAC_RESTRICT,
    tflac_u64* TFLAC_RESTRICT) = tflac_cfr_order3_std;

TFLAC_PRIVATE void (*tflac_cfr_order4)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT,
    tflac_s32* TFLAC_RESTRICT,
    tflac_u64* TFLAC_RESTRICT) = tflac_cfr_order4_std;

TFLAC_PRIVATE void (*tflac_cfr_order1_wide)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT,
    tflac_s32* TFLAC_RESTRICT,
    tflac_u64* TFLAC_RESTRICT) = tflac_cfr_order1_wide_std;

TFLAC_PRIVATE void (*tflac_cfr_order2_wide)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT,
    tflac_s32* TFLAC_RESTRICT,
    tflac_u64* TFLAC_RESTRICT) = tflac_cfr_order2_wide_std;

TFLAC_PRIVATE void (*tflac_cfr_order3_wide)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT,
    tflac_s32* TFLAC_RESTRICT,
    tflac_u64* TFLAC_RESTRICT) = tflac_cfr_order3_wide_std;

TFLAC_PRIVATE void (*tflac_cfr_order4_wide)(
    tflac_u32 blocksize,
    const tflac_s32* TFLAC_RESTRICT,
    tflac_s32* TFLAC_RESTRICT,
    tflac_u64* TFLAC_RESTRICT) = tflac_cfr_order4_wide_std;

TFLAC_PUBLIC
void tflac_detect_cpu(void) {
    int info[4];
    info[0] = 0;
    info[1] = 0;
    info[2] = 0;
    info[3] = 0;

#if defined(TFLAC_X86) || defined(TFLAC_X64)
#if defined(_MSC_VER) &&  _MSC_VER >= 1400
    __cpuid(info,1);
#elif defined(__GNUC__)
    __get_cpuid(1, (unsigned int *)&info[0], (unsigned int *)&info[1], (unsigned int *)&info[2], (unsigned int *)&info[3]);
#endif
#endif

#ifdef TFLAC_ENABLE_SSE2
    if( (info[3] & (1 << 26)) != 0) {
        tflac_cfr_order0 = tflac_cfr_order0_sse2;
        tflac_cfr_order1 = tflac_cfr_order1_sse2;
        tflac_cfr_order2 = tflac_cfr_order2_sse2;
        tflac_cfr_order3 = tflac_cfr_order3_sse2;
        tflac_cfr_order4 = tflac_cfr_order4_sse2;
    }
#endif

#ifdef TFLAC_ENABLE_SSSE3
    if( (info[2] & (1 << 9)) != 0) {
        tflac_cfr_order0 = tflac_cfr_order0_ssse3;
        tflac_cfr_order1 = tflac_cfr_order1_ssse3;
        tflac_cfr_order2 = tflac_cfr_order2_ssse3;
        tflac_cfr_order3 = tflac_cfr_order3_ssse3;
        tflac_cfr_order4 = tflac_cfr_order4_ssse3;
    }
#endif

#ifdef TFLAC_ENABLE_SSE4_1
    if( (info[2] & (1 << 19)) != 0) {
        tflac_cfr_order0 = tflac_cfr_order0_sse4_1;
        tflac_cfr_order1 = tflac_cfr_order1_sse4_1;
        tflac_cfr_order2 = tflac_cfr_order2_sse4_1;
        tflac_cfr_order3 = tflac_cfr_order3_sse4_1;
        tflac_cfr_order4 = tflac_cfr_order4_sse4_1;
    }
#endif
}

TFLAC_PUBLIC
int tflac_default_sse2(int enable) {
#ifdef TFLAC_ENABLE_SSE2
    if(enable) {
        tflac_cfr_order0 = tflac_cfr_order0_sse2;
        tflac_cfr_order1 = tflac_cfr_order1_sse2;
        tflac_cfr_order2 = tflac_cfr_order2_sse2;
        tflac_cfr_order3 = tflac_cfr_order3_sse2;
        tflac_cfr_order4 = tflac_cfr_order4_sse2;
    } else {
        tflac_cfr_order0 = tflac_cfr_order0_std;
        tflac_cfr_order1 = tflac_cfr_order1_std;
        tflac_cfr_order2 = tflac_cfr_order2_std;
        tflac_cfr_order3 = tflac_cfr_order3_std;
        tflac_cfr_order4 = tflac_cfr_order4_std;
    }
    return 0;
#else
    (void)enable;
    return 1;
#endif
}

TFLAC_PUBLIC
int tflac_default_ssse3(int enable) {
#ifdef TFLAC_ENABLE_SSSE3
    if(enable) {
        tflac_cfr_order0 = tflac_cfr_order0_ssse3;
        tflac_cfr_order1 = tflac_cfr_order1_ssse3;
        tflac_cfr_order2 = tflac_cfr_order2_ssse3;
        tflac_cfr_order3 = tflac_cfr_order3_ssse3;
        tflac_cfr_order4 = tflac_cfr_order4_ssse3;
    } else {
        tflac_cfr_order0 = tflac_cfr_order0_std;
        tflac_cfr_order1 = tflac_cfr_order1_std;
        tflac_cfr_order2 = tflac_cfr_order2_std;
        tflac_cfr_order3 = tflac_cfr_order3_std;
        tflac_cfr_order4 = tflac_cfr_order4_std;
    }
    return 0;
#else
    (void)enable;
    return 1;
#endif
}

TFLAC_PUBLIC
int tflac_default_sse4_1(int enable) {
#ifdef TFLAC_ENABLE_SSE4_1
    if(enable) {
        tflac_cfr_order0 = tflac_cfr_order0_sse4_1;
        tflac_cfr_order1 = tflac_cfr_order1_sse4_1;
        tflac_cfr_order2 = tflac_cfr_order2_sse4_1;
        tflac_cfr_order3 = tflac_cfr_order3_sse4_1;
        tflac_cfr_order4 = tflac_cfr_order4_sse4_1;
    } else {
        tflac_cfr_order0 = tflac_cfr_order0_std;
        tflac_cfr_order1 = tflac_cfr_order1_std;
        tflac_cfr_order2 = tflac_cfr_order2_std;
        tflac_cfr_order3 = tflac_cfr_order3_std;
        tflac_cfr_order4 = tflac_cfr_order4_std;
    }
    return 0;
#else
    (void)enable;
    return 1;
#endif
}

#undef TFLAC_IMPLEMENTATION
#endif /* IMPLEMENTATION */

