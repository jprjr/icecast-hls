#ifndef SAMPLEFMT_H
#define SAMPLEFMT_H

#include <stddef.h>
#include <stdint.h>

enum samplefmt {
    SAMPLEFMT_UNKNOWN = 0,
    SAMPLEFMT_U8      = 1,
    SAMPLEFMT_U8P     = 2,
    SAMPLEFMT_S16     = 3,
    SAMPLEFMT_S16P    = 4,
    SAMPLEFMT_S32     = 5,
    SAMPLEFMT_S32P    = 6,
    SAMPLEFMT_S64     = 7,
    SAMPLEFMT_S64P    = 8,
    SAMPLEFMT_FLOAT   = 9,
    SAMPLEFMT_FLOATP  = 10,
    SAMPLEFMT_DOUBLE  = 11,
    SAMPLEFMT_DOUBLEP = 12,
};

typedef enum samplefmt samplefmt;

#ifdef __cplusplus
extern "C" {
#endif

/* gets the size needed, in bytes, for a single sample */
size_t samplefmt_size(samplefmt);
int samplefmt_is_planar(samplefmt);

const char* samplefmt_str(samplefmt);

int samplefmt_convert(void* dest, const void* src, samplefmt srcfmt, samplefmt destfmt, size_t samples, size_t src_channels, size_t src_channel, size_t dest_channels, size_t dest_channel);

#define GENSIG(srctype,srctyp,desttype,desttyp) \
void samplefmt_ ## srctyp ## _to_ ## desttyp(desttype* dest, const srctype* src, size_t samples, size_t src_channels, size_t src_channel, size_t dest_channels, size_t dest_channel);

GENSIG(uint8_t, u8, uint8_t, u8)
GENSIG(uint8_t, u8, int16_t, s16)
GENSIG(uint8_t, u8, int32_t, s32)
GENSIG(uint8_t, u8, int64_t, s64)
GENSIG(uint8_t, u8, float, float)
GENSIG(uint8_t, u8, double, double)

GENSIG(int16_t, s16, uint8_t, u8)
GENSIG(int16_t, s16, int16_t, s16)
GENSIG(int16_t, s16, int32_t, s32)
GENSIG(int16_t, s16, int64_t, s64)
GENSIG(int16_t, s16, float, float)
GENSIG(int16_t, s16, double, double)

GENSIG(int32_t, s32, uint8_t, u8)
GENSIG(int32_t, s32, int16_t, s16)
GENSIG(int32_t, s32, int32_t, s32)
GENSIG(int32_t, s32, int64_t, s64)
GENSIG(int32_t, s32, float, float)
GENSIG(int32_t, s32, double, double)

GENSIG(int64_t, s64, uint8_t, u8)
GENSIG(int64_t, s64, int16_t, s16)
GENSIG(int64_t, s64, int32_t, s32)
GENSIG(int64_t, s64, int64_t, s64)
GENSIG(int64_t, s64, float, float)
GENSIG(int64_t, s64, double, double)

GENSIG(float, float, uint8_t, u8)
GENSIG(float, float, int16_t, s16)
GENSIG(float, float, int32_t, s32)
GENSIG(float, float, int64_t, s64)
GENSIG(float, float, float, float)
GENSIG(float, float, double, double)

GENSIG(double, double, uint8_t, u8)
GENSIG(double, double, int16_t, s16)
GENSIG(double, double, int32_t, s32)
GENSIG(double, double, int64_t, s64)
GENSIG(double, double, float, float)
GENSIG(double, double, double, double)

#undef GENSIG
#undef GENSIG_COPY

#ifdef __cplusplus
}
#endif

#endif
