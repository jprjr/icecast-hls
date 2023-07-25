#include "samplefmt.h"

#include <string.h>
static const char * const samplefmt_strs[] = {
    "unnkown",
    "u8",
    "u8p",
    "s16",
    "s16p",
    "s32",
    "s32p",
    "s64",
    "s64p",
    "float",
    "floatp",
    "double",
    "doublep",
    "binary"
};

const char* samplefmt_str(samplefmt f) {
    return samplefmt_strs[f];
}

int samplefmt_convert(void* dest, const void* src, samplefmt srcfmt, samplefmt destfmt,size_t samples, size_t src_channels, size_t src_channel, size_t dest_channels, size_t dest_channel) {
#define GO(fname) fname(dest,src,samples,src_channels,src_channel,dest_channels,dest_channel)

    switch(srcfmt) {

        case SAMPLEFMT_U8: /* fall-through */
        case SAMPLEFMT_U8P: {
            switch(destfmt) {
                case SAMPLEFMT_U8: /* fall-through */
                case SAMPLEFMT_U8P: GO(samplefmt_u8_to_u8); return 0;
                case SAMPLEFMT_S16: /* fall-through */
                case SAMPLEFMT_S16P: GO(samplefmt_u8_to_s16); return 0;
                case SAMPLEFMT_S32: /* fall-through */
                case SAMPLEFMT_S32P: GO(samplefmt_u8_to_s32); return 0;
                case SAMPLEFMT_S64: /* fall-through */
                case SAMPLEFMT_S64P: GO(samplefmt_u8_to_s64); return 0;
                case SAMPLEFMT_FLOAT: /* fall-through */
                case SAMPLEFMT_FLOATP: GO(samplefmt_u8_to_float); return 0;
                case SAMPLEFMT_DOUBLE: /* fall-through */
                case SAMPLEFMT_DOUBLEP: GO(samplefmt_u8_to_double); return 0;
                default: break;
            }
            break;
        }

        case SAMPLEFMT_S16: /* fall-through */
        case SAMPLEFMT_S16P: {
            switch(destfmt) {
                case SAMPLEFMT_U8: /* fall-through */
                case SAMPLEFMT_U8P: GO(samplefmt_s16_to_u8); return 0;
                case SAMPLEFMT_S16: /* fall-through */
                case SAMPLEFMT_S16P: GO(samplefmt_s16_to_s16); return 0;
                case SAMPLEFMT_S32: /* fall-through */
                case SAMPLEFMT_S32P: GO(samplefmt_s16_to_s32); return 0;
                case SAMPLEFMT_S64: /* fall-through */
                case SAMPLEFMT_S64P: GO(samplefmt_s16_to_s64); return 0;
                case SAMPLEFMT_FLOAT: /* fall-through */
                case SAMPLEFMT_FLOATP: GO(samplefmt_s16_to_float); return 0;
                case SAMPLEFMT_DOUBLE: /* fall-through */
                case SAMPLEFMT_DOUBLEP: GO(samplefmt_s16_to_double); return 0;
                default: break;
            }
            break;

        }

        case SAMPLEFMT_S32: /* fall-through */
        case SAMPLEFMT_S32P: {
            switch(destfmt) {
                case SAMPLEFMT_U8: /* fall-through */
                case SAMPLEFMT_U8P: GO(samplefmt_s32_to_u8); return 0;
                case SAMPLEFMT_S16: /* fall-through */
                case SAMPLEFMT_S16P: GO(samplefmt_s32_to_s16); return 0;
                case SAMPLEFMT_S32: /* fall-through */
                case SAMPLEFMT_S32P: GO(samplefmt_s32_to_s32); return 0;
                case SAMPLEFMT_S64: /* fall-through */
                case SAMPLEFMT_S64P: GO(samplefmt_s32_to_s64); return 0;
                case SAMPLEFMT_FLOAT: /* fall-through */
                case SAMPLEFMT_FLOATP: GO(samplefmt_s32_to_float); return 0;
                case SAMPLEFMT_DOUBLE: /* fall-through */
                case SAMPLEFMT_DOUBLEP: GO(samplefmt_s32_to_double); return 0;
                default: break;
            }
            break;
        }

        case SAMPLEFMT_S64: /* fall-through */
        case SAMPLEFMT_S64P: {
            switch(destfmt) {
                case SAMPLEFMT_U8: /* fall-through */
                case SAMPLEFMT_U8P: GO(samplefmt_s64_to_u8); return 0;
                case SAMPLEFMT_S16: /* fall-through */
                case SAMPLEFMT_S16P: GO(samplefmt_s64_to_s16); return 0;
                case SAMPLEFMT_S32: /* fall-through */
                case SAMPLEFMT_S32P: GO(samplefmt_s64_to_s32); return 0;
                case SAMPLEFMT_S64: /* fall-through */
                case SAMPLEFMT_S64P: GO(samplefmt_s64_to_s64); return 0;
                case SAMPLEFMT_FLOAT: /* fall-through */
                case SAMPLEFMT_FLOATP: GO(samplefmt_s64_to_float); return 0;
                case SAMPLEFMT_DOUBLE: /* fall-through */
                case SAMPLEFMT_DOUBLEP: GO(samplefmt_s64_to_double); return 0;
                default: break;
            }
            break;
        }

        case SAMPLEFMT_FLOAT: /* fall-through */
        case SAMPLEFMT_FLOATP: {
            switch(destfmt) {
                case SAMPLEFMT_U8: /* fall-through */
                case SAMPLEFMT_U8P: GO(samplefmt_float_to_u8); return 0;
                case SAMPLEFMT_S16: /* fall-through */
                case SAMPLEFMT_S16P: GO(samplefmt_float_to_s16); return 0;
                case SAMPLEFMT_S32: /* fall-through */
                case SAMPLEFMT_S32P: GO(samplefmt_float_to_s32); return 0;
                case SAMPLEFMT_S64: /* fall-through */
                case SAMPLEFMT_S64P: GO(samplefmt_float_to_s64); return 0;
                case SAMPLEFMT_FLOAT: /* fall-through */
                case SAMPLEFMT_FLOATP: GO(samplefmt_float_to_float); return 0;
                case SAMPLEFMT_DOUBLE: /* fall-through */
                case SAMPLEFMT_DOUBLEP: GO(samplefmt_float_to_double); return 0;
                default: break;
            }
            break;
        }

        case SAMPLEFMT_DOUBLE: /* fall-through */
        case SAMPLEFMT_DOUBLEP: {
            switch(destfmt) {
                case SAMPLEFMT_U8: /* fall-through */
                case SAMPLEFMT_U8P: GO(samplefmt_double_to_u8); return 0;
                case SAMPLEFMT_S16: /* fall-through */
                case SAMPLEFMT_S16P: GO(samplefmt_double_to_s16); return 0;
                case SAMPLEFMT_S32: /* fall-through */
                case SAMPLEFMT_S32P: GO(samplefmt_double_to_s32); return 0;
                case SAMPLEFMT_S64: /* fall-through */
                case SAMPLEFMT_S64P: GO(samplefmt_double_to_s64); return 0;
                case SAMPLEFMT_FLOAT: /* fall-through */
                case SAMPLEFMT_FLOATP: GO(samplefmt_double_to_float); return 0;
                case SAMPLEFMT_DOUBLE: /* fall-through */
                case SAMPLEFMT_DOUBLEP: GO(samplefmt_double_to_double); return 0;
                default: break;
            }
            break;
        }
        default: break;
    }
    return -1;
}


size_t samplefmt_size(samplefmt fmt) {
    switch(fmt) {
        case SAMPLEFMT_U8: /* fall-through */
        case SAMPLEFMT_U8P: return sizeof(uint8_t);
        case SAMPLEFMT_S16: /* fall-through */
        case SAMPLEFMT_S16P: return sizeof(int16_t);
        case SAMPLEFMT_S32: /* fall-through */
        case SAMPLEFMT_S32P: return sizeof(int32_t);
        case SAMPLEFMT_FLOAT: /* fall-through */
        case SAMPLEFMT_FLOATP: return sizeof(float);
        case SAMPLEFMT_S64: /* fall-through */
        case SAMPLEFMT_S64P: return sizeof(int64_t);
        case SAMPLEFMT_DOUBLE: /* fall-through */
        case SAMPLEFMT_DOUBLEP: return sizeof(double);
        default: break;
    }
    return 0;
}

int samplefmt_is_planar(samplefmt fmt) {
    switch(fmt) {
        case SAMPLEFMT_U8P: /* fall-through */
        case SAMPLEFMT_S16P: /* fall-through */
        case SAMPLEFMT_S32P: /* fall-through */
        case SAMPLEFMT_FLOATP: /* fall-through */
        case SAMPLEFMT_S64P: /* fall-through */
        case SAMPLEFMT_DOUBLEP: return 1;
        default: break;
    }
    return 0;
}

#define GENFUNC(srctype,srctyp,desttype,desttyp,tmptype,conv,clamp) \
void samplefmt_ ## srctyp ## _to_ ## desttyp(desttype* dest, const srctype* src, size_t samples, size_t src_channels, size_t src_channel, size_t dest_channels, size_t dest_channel) { \
    size_t i = 0; \
    tmptype t; \
    for(i=0;i<samples;i++) { \
        t = (tmptype)src[(i*src_channels) + src_channel ]; \
        t = conv(t); \
        dest[(i*dest_channels) + dest_channel] = (desttype)(clamp(t)); \
    } \
}

#define noclamp(val) val

#define conv_u8_u8(t) (t)
#define conv_u8_s16(t) (t - 0x80) * (1 << 8)
#define conv_u8_s32(t) (t - 0x80) * (1 << 24)
#define conv_u8_s64(t) (t - 0x80) * (((uint64_t)1) << 56)
#define conv_u8_float(t) (t - 128.0f) / 128.0f
#define conv_u8_double(t) conv_u8_float(t)


#define conv_s16_u8(t) ((t / (1 << 8)) + 0x80)
#define conv_s16_s16(t) (t)
#define conv_s16_s32(t) (t * (1<<16))
#define conv_s16_s64(t) (t * (((uint64_t)1)<<48))
#define conv_s16_float(t) (t / 32768.0f)
#define conv_s16_double(t) conv_s16_float(t)

#define conv_s32_u8(t) ((t / (1 << 24)) + 0x80)
#define conv_s32_s16(t) (t / (1 << 16))
#define conv_s32_s32(t) (t)
#define conv_s32_s64(t) (t * (((uint64_t)1) << 32))
#define conv_s32_float(t) (t / 2147483648.0f)
#define conv_s32_double(t) conv_s32_float(t)

#define conv_s64_u8(t) ((t / (((uint64_t)1) << 56)) + 0x80)
#define conv_s64_s16(t) (t / (((uint64_t)1) << 48))
#define conv_s64_s32(t) (t / (((uint64_t)1) << 32))
#define conv_s64_s64(t) (t)
#define conv_s64_float(t) (t / 9223372036854775808.0f)
#define conv_s64_double(t) conv_s64_float(t)

#define conv_float_u8(t) ( (t * 255.0f) - 128.0f )
#define conv_float_s16(t) (t * 32768.0f)
#define conv_float_s32(t) (t * 2147483648.0f)
#define conv_float_s64(t) (t * 9223372036854775808.0f)
#define conv_float_float(t) (t)
#define conv_float_double(t) (t)

#define conv_double_u8(t) conv_float_u8(t)
#define conv_double_s16(t) conv_float_s16(t)
#define conv_double_s32(t) conv_float_s32(t)
#define conv_double_s64(t) conv_float_s64(t)
#define conv_double_float(t) (t)
#define conv_double_double(t) (t)

#define clamp_u8(val) (val < 0.0f ? 0.0f : val > 255.0f ? 255.0f : val)
#define clamp_s16(val) (val < -32768.0f ? -32768.0f : val > 32767.0f ? 32767.0f : val)
#define clamp_s32(val) ( val < - 2147483648.0f ?  2147483648.0f : val > 2147483647.0f ? 2147483647.0f : val)
#define clamp_s64(val) ( val < -9223372036854775808.0f ? -9223372036854775808.0f : val > 9223372036854775807.0f ? 9223372036854775807.0f : val)


GENFUNC(uint8_t, u8, uint8_t,  u8, int16_t, conv_u8_u8, noclamp)
GENFUNC(uint8_t, u8, int16_t, s16, int16_t, conv_u8_s16, noclamp)
GENFUNC(uint8_t, u8, int32_t, s32, int32_t, conv_u8_s32, noclamp)
GENFUNC(uint8_t, u8, int64_t, s64, int64_t, conv_u8_s64, noclamp)
GENFUNC(uint8_t, u8, float, float, double, conv_u8_float, noclamp)
GENFUNC(uint8_t, u8, double, double, double, conv_u8_float, noclamp)

GENFUNC(int16_t, s16, uint8_t, u8,  int16_t, conv_s16_u8, noclamp)
GENFUNC(int16_t, s16, int16_t, s16, int32_t, conv_s16_s16, noclamp)
GENFUNC(int16_t, s16, int32_t, s32, int32_t, conv_s16_s32, noclamp)
GENFUNC(int16_t, s16, int64_t, s64, int64_t, conv_s16_s64, noclamp)
GENFUNC(int16_t, s16, float, float, double, conv_s16_float, noclamp)
GENFUNC(int16_t, s16, double, double, double, conv_s16_double, noclamp)

GENFUNC(int32_t, s32, uint8_t, u8,  int32_t, conv_s32_u8, noclamp)
GENFUNC(int32_t, s32, int16_t, s16, int32_t, conv_s32_s16, noclamp)
GENFUNC(int32_t, s32, int32_t, s32, int32_t, conv_s32_s32, noclamp)
GENFUNC(int32_t, s32, int64_t, s64, int64_t, conv_s32_s64, noclamp)
GENFUNC(int32_t, s32, float, float, double, conv_s32_float, noclamp)
GENFUNC(int32_t, s32, double, double, double, conv_s32_double, noclamp)

GENFUNC(int64_t, s64, uint8_t, u8,  int64_t, conv_s64_u8, noclamp)
GENFUNC(int64_t, s64, int16_t, s16, int64_t, conv_s64_s16, noclamp)
GENFUNC(int64_t, s64, int32_t, s32, int64_t, conv_s64_s32, noclamp)
GENFUNC(int64_t, s64, int64_t, s64, int64_t, conv_s64_s64, noclamp)
GENFUNC(int64_t, s64, float, float, double, conv_s64_float, noclamp)
GENFUNC(int64_t, s64, double, double, double, conv_s64_double, noclamp)

GENFUNC(float, float, uint8_t, u8,  double, conv_float_u8,  clamp_u8)
GENFUNC(float, float, int16_t, s16, double, conv_float_s16, clamp_s16)
GENFUNC(float, float, int32_t, s32, double, conv_float_s32, clamp_s32)
GENFUNC(float, float, int64_t, s64, double, conv_float_s64, clamp_s64)
GENFUNC(float, float, float, float, float, conv_float_float, noclamp)
GENFUNC(float, float, double, double, double, conv_float_double, noclamp)

GENFUNC(double, double, uint8_t, u8,  double, conv_double_u8,  clamp_u8)
GENFUNC(double, double, int16_t, s16, double, conv_double_s16, clamp_s16)
GENFUNC(double, double, int32_t, s32, double, conv_double_s32, clamp_s32)
GENFUNC(double, double, int64_t, s64, double, conv_double_s64, clamp_s64)
GENFUNC(double, double, float, float, double, conv_float_double, noclamp)
GENFUNC(double, double, double, double, double, conv_double_double, noclamp)
