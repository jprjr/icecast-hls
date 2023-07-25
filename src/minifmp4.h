/* SPDX-License-Identifier: 0BSD */
#ifndef MINIFMP4_H
#define MINIFMP4_H

/*

MINIFMP4
========

This is a small, single-header library for writing fragmented
MP4/ISO Base Media File Format files. It's designed to be
fairly simple to use, and not necessarily know all the inner
workings of how MP4 files work.

Usage
-----

In exactly one source file, define MINIFMP4_IMPLEMENTATION before
including the header:

```c
  #define MINIFMP4_IMPLEMENTATION
  #include "minifmp4.h"
```

The general workflow is:

1. Allocate and initialize 1 fmp4_mux object.
2. Configure your file-level options on the fmp4_mux object,
   things like the MP4 brand, minor version, and compatible
   brands.
3. Allocate and initialize 1+ fmp4_track objects and add them to the fmp4_mux
   object.
4. Configure your track-level options on your fmp4_track
   objects, things like the type of media, default flags,
   decoder-specific information, and so on.
5. Write out your initialization segment.
6. Add samples to the fmp4_track objects.
7. Write out media segments.
8. Close and free your objects.

The library allows a variety of memory-management options. You
can use the default malloc/realloc/free allocators, or supply a
custom set. You can allocate the needed structs on your own
and initialize them, or have the library allocate them for you.

All structs have getter/setter functions, a goal is to be able to use
this library without accessing a single struct field. Some fields
require using the getter/setter functions. Generally-speaking,
if a field's type is fmp4_membuf, you'll need to use a getter/setter function.

My main focus is on audio, so that's all the library supports
right now. It has features like, the ability to add loudness
metadata and send EventMessage objects.

Take a look at the demos folder for a few examples.

License
------

BSD Zero Clause, details at the end of this file.
*/
#if !defined(FMP4_API)
    #ifdef FMP4_DLL
        #ifdef _WIN32
            #define FMP4_DLL_IMPORT  __declspec(dllimport)
            #define FMP4_DLL_EXPORT  __declspec(dllexport)
            #define FMP4_DLL_PRIVATE static
        #else
            #if defined(__GNUC__) && __GNUC__ >= 4
                #define FMP4_DLL_IMPORT  __attribute__((visibility("default")))
                #define FMP4_DLL_EXPORT  __attribute__((visibility("default")))
                #define FMP4_DLL_PRIVATE __attribute__((visibility("hidden")))
            #else
                #define FMP4_DLL_IMPORT
                #define FMP4_DLL_EXPORT
                #define FMP4_DLL_PRIVATE static
            #endif
        #endif

        #ifdef MINIFMP4_IMPLEMENTATION
            #define FMP4_API  FMP4_DLL_EXPORT
        #else
            #define FMP4_API  FMP4_DLL_IMPORT
        #endif
        #define FMP4_PRIVATE FMP4_DLL_PRIVATE
    #else
        #define FMP4_API extern
        #define FMP4_PRIVATE static
    #endif
#endif

enum fmp4_result {
    FMP4_CHANNELSINVALID    = -23,
    FMP4_TIMESCALEINVALID   = -22,
    FMP4_EMSGSCHEMENOTSET   = -21,
    FMP4_EMSGVALUENOTSET    = -20,
    FMP4_EMSGMESSAGENOTSET  = -19,
    FMP4_LOUDNESSNOTSET     = -18,
    FMP4_METHODINVALID      = -17,
    FMP4_RELIABILITYINVALID = -16,
    FMP4_SYSTEMINVALID      = -15,
    FMP4_PEAKINVALID        = -14,
    FMP4_NOSAMPLES          = -13,
    FMP4_NOTRACKS           = -12,
    FMP4_MEMUNDERFLOW       = -11,
    FMP4_INVALIDEMSGVER     = -10,
    FMP4_MISSINGDSI         = -9,
    FMP4_OBJECTINVALID      = -8,
    FMP4_CODECINVALID       = -7,
    FMP4_STREAMINVALID      = -6,
    FMP4_ESTAGTOOBIG        = -5,
    FMP4_BOXTOOBIG          = -4,
    FMP4_STACKERROR         = -3,
    FMP4_WRITEERR           = -2,
    FMP4_OUTOFMEM           = -1,
    FMP4_OK                 =  0,
};

typedef enum fmp4_result fmp4_result;


#include <stddef.h>
#include <stdint.h>


/* callback used in the file-writing functions */
typedef size_t (*fmp4_write_cb)(const void* src, size_t len, void* userdata);

/* callbacks used for custom memory allocators */
typedef void* (*fmp4_malloc_cb)(size_t len, void* userdata);
typedef void* (*fmp4_realloc_cb)(void* ptr, size_t len, void* userdata);
typedef void (*fmp4_free_cb)(void* ptr, void* userdata);

struct fmp4_allocator {
    void* userdata;
    fmp4_malloc_cb malloc;
    fmp4_realloc_cb realloc;
    fmp4_free_cb free;
};

typedef struct fmp4_allocator fmp4_allocator;

/* stuff for managing memory buffers, kind of like a vector */
struct fmp4_membuf {
    uint8_t* x;
    size_t len;
    size_t a;
    const fmp4_allocator* allocator;
};

typedef struct fmp4_membuf fmp4_membuf;

enum fmp4_stream_type {
    FMP4_STREAM_TYPE_FORBIDDEN = 0x00,
    FMP4_STREAM_TYPE_AUDIO = 0x05,
};

typedef enum fmp4_stream_type fmp4_stream_type;

enum fmp4_roll_type {
    FMP4_ROLL_TYPE_ROLL,
    FMP4_ROLL_TYPE_PROL,
};

typedef enum fmp4_roll_type fmp4_roll_type;

enum fmp4_object_type {
    FMP4_OBJECT_TYPE_FORBIDDEN = 0x00,
    FMP4_OBJECT_TYPE_AAC = 0x40, /* covers AAC, HE-AAC, xHE-AAC, etc */
    FMP4_OBJECT_TYPE_MP3 = 0x6B,
};

typedef enum fmp4_object_type fmp4_object_type;

enum fmp4_codec {
    FMP4_CODEC_UNDEFINED = 0x00000000,
    FMP4_CODEC_MP4A = 0x6d703461, /* covers AAC, HE-AAC, xHE-AAC, MP3-in-MP4, etc */
    FMP4_CODEC_ALAC = 0x616c6163,
    FMP4_CODEC_FLAC = 0x664c6143,
    FMP4_CODEC_OPUS = 0x4f707573,
    FMP4_CODEC_AC3  = 0x61632d33,
    FMP4_CODEC_EAC3 = 0x65632d33,
};

typedef enum fmp4_codec fmp4_codec;

struct fmp4_measurement {
    uint8_t method; /* methodDefinition */
    uint8_t value; /* methodValue */
    uint8_t system; /*measurementSystem */
    uint8_t reliability;
    const fmp4_allocator* allocator;
};

typedef struct fmp4_measurement fmp4_measurement;

enum fmp4_loudness_type {
    FMP4_LOUDNESS_UNDEF = 0,
    FMP4_LOUDNESS_TRACK = 1,
    FMP4_LOUDNESS_ALBUM = 2,
};

typedef enum fmp4_loudness_type fmp4_loudness_type;

struct fmp4_loudness {
    fmp4_loudness_type type;
    uint8_t downmix_id; /* downmix_ID */
    uint8_t drc_id; /* DRC_set_ID */
    int16_t sample_peak; /*bsSamplePeakLevel */
    int16_t true_peak; /* bsTruePeakLevel */
    uint8_t system; /*measurement_system_for_TP */
    uint8_t reliability; /* reliaibility_for_TP */
    fmp4_membuf measurements;
    fmp4_membuf alloc_measurement;
    const fmp4_allocator* allocator;
};

typedef struct fmp4_loudness fmp4_loudness;

struct fmp4_sample_flags {
    uint8_t is_leading;
    uint8_t depends_on;
    uint8_t is_depended_on;
    uint8_t has_redundancy;
    uint8_t padding_value;
    uint8_t is_non_sync;
    uint16_t degradation_priority;
    const fmp4_allocator* allocator;
};

typedef struct fmp4_sample_flags fmp4_sample_flags;

struct fmp4_sample_info {
    uint32_t duration;
    uint32_t size;
    uint32_t sample_group; /* used when you've specified a roll distance */
    fmp4_sample_flags flags;
    const fmp4_allocator* allocator;
};

typedef struct fmp4_sample_info fmp4_sample_info;

struct fmp4_emsg {
    uint8_t version;
    uint32_t timescale;
    uint32_t presentation_time_delta;
    uint64_t presentation_time;
    uint32_t event_duration;
    uint32_t id;
    fmp4_membuf scheme_id_uri;
    fmp4_membuf value;
    fmp4_membuf message;
    const fmp4_allocator* allocator;
};

typedef struct fmp4_emsg fmp4_emsg;

struct fmp4_track {
    fmp4_stream_type stream_type;
    fmp4_codec codec;
    fmp4_object_type object_type;
    uint64_t base_media_decode_time;
    uint32_t time_scale;
    uint8_t language[4];
    uint32_t encoder_delay;
    int16_t roll_distance;
    fmp4_roll_type roll_type;

    union {
        struct {
            uint16_t channels;
        } audio;
    } info;

    fmp4_sample_info default_sample_info;
    fmp4_membuf loudness;
    fmp4_membuf dsi;

    fmp4_membuf alloc_loudness;

    uint32_t first_sample_flags;

    uint8_t  trun_sample_flags_set;
    uint32_t trun_sample_flags;

    uint8_t  trun_sample_duration_set;
    uint32_t trun_sample_duration;

    uint8_t  trun_sample_size_set;
    uint32_t trun_sample_size;

    uint64_t trun_sample_count;
    size_t data_offset_pos;

    fmp4_membuf sample_info;
    fmp4_membuf mdat;
    const fmp4_allocator* allocator;
};

typedef struct fmp4_track fmp4_track;

struct fmp4_mux {
    char brand_major[4];
    uint32_t brand_minor_version;

    fmp4_membuf buffer;
    fmp4_membuf stack;
    fmp4_membuf brands;
    fmp4_membuf tracks;
    fmp4_membuf emsgs;

    fmp4_membuf alloc_track;
    fmp4_membuf alloc_emsg;

    size_t moof_offset;
    uint32_t fragments;
    const fmp4_allocator* allocator;
};

typedef struct fmp4_mux fmp4_mux;

#ifdef __cplusplus
extern "C" {
#endif


/* struct initialization functions */
FMP4_API
void
fmp4_mux_init(fmp4_mux* mux, const fmp4_allocator* allocator);

FMP4_API
void
fmp4_track_init(fmp4_track* track, const fmp4_allocator* allocator);

FMP4_API
void
fmp4_sample_info_init(fmp4_sample_info* sample_info);

FMP4_API
void
fmp4_sample_flags_init(fmp4_sample_flags* sample_flags);

FMP4_API
void
fmp4_loudness_init(fmp4_loudness* loudness, const fmp4_allocator* allocator);

FMP4_API
void
fmp4_measurement_init(fmp4_measurement* measurement);

FMP4_API
void
fmp4_emsg_init(fmp4_emsg* emsg, const fmp4_allocator* allocator);

/* struct allocation functions (pre-initialized) */
FMP4_API
fmp4_mux*
fmp4_mux_new(const fmp4_allocator* allocator);

FMP4_API
fmp4_track*
fmp4_track_new(const fmp4_allocator* allocator);

FMP4_API
fmp4_sample_info*
fmp4_sample_info_new(const fmp4_allocator* allocator);

FMP4_API
fmp4_sample_flags*
fmp4_sample_flags_new(const fmp4_allocator* allocator);

FMP4_API
fmp4_loudness*
fmp4_loudness_new(const fmp4_allocator* allocator);

FMP4_API
fmp4_measurement*
fmp4_measurement_new(const fmp4_allocator* allocator);

FMP4_API
fmp4_emsg*
fmp4_emsg_new(const fmp4_allocator* allocator);


/* allocates and adds a new track to the muxer, it will
 * automatically be freed whenever you close/free the muxer */
FMP4_API
fmp4_track*
fmp4_mux_new_track(fmp4_mux* mux);

/* allocates an emsg, does NOT add it to the next media
 * segment write. It will automatically be freed whenever you
 * close/free the muxer */
FMP4_API
fmp4_emsg*
fmp4_mux_new_emsg(fmp4_mux* mux);

/* adds an already-allocated track to the muxer, stores a reference to the track pointer,
 * you'll have to deallocate/free the track on your own */
FMP4_API
fmp4_result
fmp4_mux_add_track(fmp4_mux* mux, const fmp4_track* track);

/* adds an emsg to the muxer for the next segment write - the
 * emsg isn't serialized until you write the segment, so you can add the emsg
 * and update fields, add data, etc up until you call fmp4_mux_write_segment */
FMP4_API
fmp4_result
fmp4_mux_add_emsg(fmp4_mux* mux, const fmp4_emsg* emsg);

/* validates we have everything needed to write out an initialization segment */
FMP4_API
fmp4_result
fmp4_mux_validate_init(const fmp4_mux* mux);

/* write out an initialization segment */
FMP4_API
fmp4_result
fmp4_mux_write_init(fmp4_mux* mux, fmp4_write_cb write, void* userdata);

/* validates we have everything needed to write out a media segment */
FMP4_API
fmp4_result
fmp4_mux_validate_segment(const fmp4_mux* mux);

/* write out a media segment - this will clear all buffered samples from
 * tracks, and remove emsg references */
FMP4_API
fmp4_result
fmp4_mux_write_segment(fmp4_mux* mux, fmp4_write_cb write, void* userdata);

/* sets the major brand */
FMP4_API
void
fmp4_mux_set_brand_major(fmp4_mux* mux, const char brand[4], uint32_t ver);

/* add a brand to the compatible brand list - the major brand
 * is automatically included */
FMP4_API
fmp4_result
fmp4_mux_add_brand(fmp4_mux* mux, const char brand[4]);



/* buffer a sample - the date and sample_info are buffered, it's safe to free or re-use
 * those pointers after calling this */
FMP4_API
fmp4_result
fmp4_track_add_sample(fmp4_track* track, const void* data, const fmp4_sample_info* sample);

/* allocate a new fmp4_loudness pointer and add it to the track, you'll need
 * to free the pointer whenever you're done with it. It's safe to free it after
 * calling fmp4_mux_write_init - it won't be used again */
FMP4_API
fmp4_loudness*
fmp4_track_new_loudness(fmp4_track* track);

/* add an already-allocated loudness pointer to the track */
FMP4_API
fmp4_result
fmp4_track_add_loudness(fmp4_track* track, const fmp4_loudness* loudness);

/* validates the track has valid init-related data */
FMP4_API
fmp4_result
fmp4_track_validate_init(const fmp4_track* track);

/* validates the track has valid segment-related data */
FMP4_API
fmp4_result
fmp4_track_validate_segment(const fmp4_track* track);



/* allocates a new loudness measurement, and adds it to the loudness struct */
FMP4_API
fmp4_measurement*
fmp4_loudness_new_measurement(fmp4_loudness* loudness);

/* add a pre-allocated measurement to a loudness */
FMP4_API
fmp4_result
fmp4_loudness_add_measurement(fmp4_loudness* loudness, const fmp4_measurement* measurement);

/* validates a loudness */
FMP4_API
fmp4_result
fmp4_loudness_validate(const fmp4_loudness* loudness);


FMP4_API
fmp4_result
fmp4_measurement_validate(const fmp4_measurement* measurement);


FMP4_API
fmp4_result
fmp4_emsg_validate(const fmp4_emsg* emsg);



/* struct closure functions */

/* frees all allocated data used by the muxer - like buffer/scratch space,
 * all tracks allocated via fmp4_mux_new_track,
 * all emsgs allocated via fmp4_mux_new_emsg */

FMP4_API
void
fmp4_mux_close(fmp4_mux* mux);

/* frees all allocated data used by the track - dsi and samples,
 * and any loudness allocated via fmp4_track_new_loudness */
FMP4_API
void
fmp4_track_close(fmp4_track* track);

/* frees allocated data used by the loudness, and any
 * measurements allocated via fmp4_loudness_new_measurement */
FMP4_API
void
fmp4_loudness_close(fmp4_loudness* loudness);

/* frees allocated data used by the emsg */
FMP4_API
void
fmp4_emsg_close(fmp4_emsg* emsg);

/* closes and frees */
FMP4_API
void
fmp4_mux_free(fmp4_mux* mux);

FMP4_API
void
fmp4_track_free(fmp4_track *track);

FMP4_API
void
fmp4_sample_info_free(fmp4_sample_info* sample_info);

FMP4_API
void
fmp4_sample_flags_free(fmp4_sample_flags* sample_flags);

FMP4_API
void
fmp4_loudness_free(fmp4_loudness* loudness);

FMP4_API
void
fmp4_measurement_free(fmp4_measurement* measurement);

FMP4_API
void
fmp4_emsg_free(fmp4_emsg* emsg);



/* functions for querying the size of structures
 * at runtime instead of compile-time */

FMP4_API
size_t
fmp4_mux_size(void);

FMP4_API
size_t
fmp4_track_size(void);

FMP4_API
size_t
fmp4_sample_info_size(void);

FMP4_API
size_t
fmp4_loudness_size(void);

FMP4_API
size_t
fmp4_measurement_size(void);

FMP4_API
size_t
fmp4_emsg_size(void);


/* you probably just want to get various pieces of data directly but,
 * if you've embedded this in some kind of FFI environment where
 * using data structures is tricky, there's also get functions */

FMP4_API
fmp4_result
fmp4_mux_get_brand_major(const fmp4_mux* mux, char brand[4], uint32_t* ver);

FMP4_API
uint32_t
fmp4_mux_get_brand_minor_version(const fmp4_mux* mux);

FMP4_API
const char*
fmp4_mux_get_brands(const fmp4_mux* mux, size_t* len);

FMP4_API
size_t
fmp4_mux_get_track_count(const fmp4_mux* mux);

FMP4_API
fmp4_track*
fmp4_mux_get_track(const fmp4_mux* mux, size_t index);

FMP4_API
fmp4_track**
fmp4_mux_get_tracks(const fmp4_mux* mux, size_t* count);

FMP4_API
fmp4_stream_type
fmp4_track_get_stream_type(const fmp4_track* track);

FMP4_API
fmp4_codec
fmp4_track_get_codec(const fmp4_track* track);

FMP4_API
fmp4_object_type
fmp4_track_get_object_type(const fmp4_track* track);

FMP4_API
uint64_t
fmp4_track_get_base_media_decode_time(const fmp4_track* track);

FMP4_API
uint32_t
fmp4_track_get_time_scale(const fmp4_track* track);
#define fmp4_track_get_audio_sample_rate(track) fmp4_track_get_time_scale(track)

FMP4_API
const char*
fmp4_track_get_language(const fmp4_track* track);

FMP4_API
uint16_t
fmp4_track_get_audio_channels(const fmp4_track* track);

FMP4_API
uint32_t
fmp4_track_get_encoder_delay(const fmp4_track* track);

FMP4_API
int16_t
fmp4_track_get_roll_distance(const fmp4_track* track);

FMP4_API
fmp4_roll_type
fmp4_track_get_roll_type(const fmp4_track* track);

FMP4_API
fmp4_result
fmp4_track_get_default_sample_info(const fmp4_track* track, fmp4_sample_info* info);

FMP4_API
size_t
fmp4_track_get_loudness_count(const fmp4_track* track);

FMP4_API
fmp4_loudness*
fmp4_track_get_loudness(const fmp4_track* track, size_t index);

FMP4_API
fmp4_loudness**
fmp4_track_get_loudnesses(const fmp4_track* track, size_t* count);

FMP4_API
size_t
fmp4_track_get_sample_info_count(const fmp4_track* track);

FMP4_API
fmp4_sample_info*
fmp4_track_get_sample_info(const fmp4_track* track, size_t index);

FMP4_API
fmp4_sample_info**
fmp4_track_get_sample_infos(const fmp4_track* track, size_t* count);

FMP4_API
const void*
fmp4_track_get_dsi(const fmp4_track* track, size_t* len);

FMP4_API
fmp4_result
fmp4_track_get_first_sample_flags(const fmp4_track* track, fmp4_sample_flags* flags);

FMP4_API
uint8_t
fmp4_track_get_trun_sample_flags_set(const fmp4_track* track);

FMP4_API
fmp4_result
fmp4_track_get_trun_sample_flags(const fmp4_track* track, fmp4_sample_flags* flags);

FMP4_API
uint8_t
fmp4_track_get_trun_sample_duration_set(const fmp4_track* track);

FMP4_API
uint32_t
fmp4_track_get_trun_sample_duration(const fmp4_track* track);

FMP4_API
uint8_t
fmp4_track_get_trun_sample_size_set(const fmp4_track* track);

FMP4_API
uint32_t
fmp4_track_get_trun_sample_size(const fmp4_track* track);

FMP4_API
uint64_t
fmp4_track_get_trun_sample_count(const fmp4_track* track);

FMP4_API
fmp4_loudness_type
fmp4_loudness_get_type(const fmp4_loudness* loudness);

FMP4_API
uint8_t
fmp4_loudness_get_downmix_id(const fmp4_loudness* loudness);

FMP4_API
uint8_t
fmp4_loudness_get_drc_id(const fmp4_loudness* loudness);

FMP4_API
double
fmp4_loudness_get_sample_peak(const fmp4_loudness* loudness);

FMP4_API
double
fmp4_loudness_get_rue_peak(const fmp4_loudness* loudness);

FMP4_API
uint8_t
fmp4_loudness_get_system(const fmp4_loudness* loudness);

FMP4_API
uint8_t
fmp4_loudness_get_reliability(const fmp4_loudness* loudness);

FMP4_API
size_t
fmp4_loudness_get_measurement_count(const fmp4_loudness* loudness);

FMP4_API
fmp4_measurement*
fmp4_loudness_get_measurement(const fmp4_loudness* loudness, size_t index);

FMP4_API
fmp4_measurement**
fmp4_loudness_get_measurements(const fmp4_loudness* loudness, size_t* count);

FMP4_API
uint8_t
fmp4_measurement_get_method(const fmp4_measurement* measurement);

FMP4_API
double
fmp4_measurement_get_value(const fmp4_measurement* measurement);

FMP4_API
uint8_t
fmp4_measurement_get_system(const fmp4_measurement* measurement);

FMP4_API
uint8_t
fmp4_measurement_get_reliability(const fmp4_measurement* measurement);

FMP4_API
uint8_t
fmp4_sample_flags_get_is_leading(const fmp4_sample_flags* flags);

FMP4_API
uint8_t
fmp4_sample_flags_get_depends_on(const fmp4_sample_flags* flags);

FMP4_API
uint8_t
fmp4_sample_flags_get_is_depended_on(const fmp4_sample_flags* flags);

FMP4_API
uint8_t
fmp4_sample_flags_get_has_redundancy(const fmp4_sample_flags* flags);

FMP4_API
uint8_t
fmp4_sample_flags_get_padding_value(const fmp4_sample_flags* flags);

FMP4_API
uint8_t
fmp4_sample_flags_get_is_non_sync(const fmp4_sample_flags* flags);

FMP4_API
uint16_t
fmp4_sample_flags_get_degradation_priority(const fmp4_sample_flags* flags);

FMP4_API
uint32_t
fmp4_sample_info_get_duration(const fmp4_sample_info* info);

FMP4_API
uint32_t
fmp4_sample_info_get_size(const fmp4_sample_info* info);

FMP4_API
fmp4_result
fmp4_sample_info_get_flags(const fmp4_sample_info* info, fmp4_sample_flags* flags);

FMP4_API
uint8_t
fmp4_emsg_get_version(const fmp4_emsg* emsg);

FMP4_API
uint32_t
fmp4_emsg_get_timescale(const fmp4_emsg* emsg);

FMP4_API
uint32_t
fmp4_emsg_get_presentation_time_delta(const fmp4_emsg* emsg);

FMP4_API
uint64_t
fmp4_emsg_get_presentation_time(const fmp4_emsg* emsg);

FMP4_API
uint32_t
fmp4_emsg_get_event_duration(const fmp4_emsg* emsg);

FMP4_API
uint32_t
fmp4_emsg_get_id(const fmp4_emsg* emsg);

FMP4_API
const char*
fmp4_emsg_get_scheme_id_uri(const fmp4_emsg* emsg);

FMP4_API
const char*
fmp4_emsg_get_value(const fmp4_emsg* emsg);

FMP4_API
const uint8_t*
fmp4_emsg_get_message(const fmp4_emsg* emsg, uint32_t* message_size);

FMP4_API
uint32_t
fmp4_emsg_get_message_size(const fmp4_emsg* emsg);




/* you probably just want to set various pieces of data directly but,
 * if you've embedded this in some kind of FFI environment where
 * using data structures is tricky, there's also set functions */

FMP4_API
void
fmp4_track_set_stream_type(fmp4_track* track, fmp4_stream_type stream_type);

FMP4_API
void
fmp4_track_set_codec(fmp4_track* track, fmp4_codec codec);

FMP4_API
void
fmp4_track_set_object_type(fmp4_track* track, fmp4_object_type object_type);

FMP4_API
void
fmp4_track_set_base_media_decode_time(fmp4_track* track, uint64_t base_media_decode_time);

FMP4_API
void
fmp4_track_set_time_scale(fmp4_track* track, uint32_t time_scale);

FMP4_API
void
fmp4_track_set_language(fmp4_track* track, const char* language);

#define fmp4_track_set_audio_sample_rate(t, r) fmp4_track_set_time_scale(t, r)

FMP4_API
void
fmp4_track_set_encoder_delay(fmp4_track* track, uint32_t delay);

FMP4_API
void
fmp4_track_set_roll_distance(fmp4_track* track, int16_t distance);

FMP4_API
void
fmp4_track_set_roll_type(fmp4_track* track, fmp4_roll_type roll_type);

FMP4_API
void
fmp4_track_set_audio_channels(fmp4_track* track, uint16_t channels);

FMP4_API
fmp4_result
fmp4_track_set_dsi(fmp4_track* track, const void* dsi, size_t len);

FMP4_API
void
fmp4_track_set_default_sample_info(fmp4_track *track, const fmp4_sample_info* info);

FMP4_API
void
fmp4_track_set_first_sample_flags(fmp4_track* track, const fmp4_sample_flags* flags);

FMP4_API
void
fmp4_track_set_trun_sample_flags(fmp4_track* track, const fmp4_sample_flags* flags);

FMP4_API
void
fmp4_track_set_trun_sample_duration(fmp4_track* track, uint32_t duration);

FMP4_API
void
fmp4_track_set_trun_sample_size(fmp4_track* track, uint32_t size);

FMP4_API
void
fmp4_track_set_trun_sample_count(fmp4_track* track, uint64_t count);

FMP4_API
void
fmp4_loudness_set_downmix_id(fmp4_loudness* loudness, uint8_t id);

FMP4_API
void
fmp4_loudness_set_drc_id(fmp4_loudness* loudness, uint8_t id);

FMP4_API
void
fmp4_loudness_set_type(fmp4_loudness* loudness, fmp4_loudness_type type);

FMP4_API
fmp4_result
fmp4_loudness_set_sample_peak(fmp4_loudness* loudness, double peak);

FMP4_API
fmp4_result
fmp4_loudness_set_true_peak(fmp4_loudness* loudness, double peak);

FMP4_API
fmp4_result
fmp4_loudness_set_system(fmp4_loudness* loudness, uint8_t system);

FMP4_API
fmp4_result
fmp4_loudness_set_reliability(fmp4_loudness* loudness, uint8_t reliability);

FMP4_API
fmp4_result
fmp4_measurement_set_method(fmp4_measurement* measurement, uint8_t method);

FMP4_API
fmp4_result
fmp4_measurement_set_value(fmp4_measurement* measurement, double value);

FMP4_API
fmp4_result
fmp4_measurement_set_system(fmp4_measurement* measurement, uint8_t system);

FMP4_API
fmp4_result
fmp4_measurement_set_reliability(fmp4_measurement* measurement, uint8_t reliability);

FMP4_API
void
fmp4_sample_flags_set_is_leading(fmp4_sample_flags* flags, uint8_t is_leading);

FMP4_API
void
fmp4_sample_flags_set_depends_on(fmp4_sample_flags* flags, uint8_t depends_on);

FMP4_API
void
fmp4_sample_flags_set_is_depended_on(fmp4_sample_flags* flags, uint8_t is_depended_on);

FMP4_API
void
fmp4_sample_flags_set_has_redundancy(fmp4_sample_flags* flags, uint8_t has_redundancy);

FMP4_API
void
fmp4_sample_flags_set_padding_value(fmp4_sample_flags* flags, uint8_t padding_value);

FMP4_API
void
fmp4_sample_flags_set_is_non_sync(fmp4_sample_flags* flags, uint8_t is_non_sync);

FMP4_API
void
fmp4_sample_flags_set_degradation_priority(fmp4_sample_flags* flags, uint16_t priority);

FMP4_API
void
fmp4_sample_info_set_duration(fmp4_sample_info* info, uint32_t duration);

FMP4_API
void
fmp4_sample_info_set_size(fmp4_sample_info* info, uint32_t size);

FMP4_API
void
fmp4_sample_info_set_flags(fmp4_sample_info* info, const fmp4_sample_flags* flags);

FMP4_API
fmp4_result
fmp4_emsg_set_version(fmp4_emsg* emsg, uint8_t version);

FMP4_API
void
fmp4_emsg_set_timescale(fmp4_emsg* emsg, uint32_t timescale);

FMP4_API
void
fmp4_emsg_set_presentation_time_delta(fmp4_emsg* emsg, uint32_t presentation_time_delta);

FMP4_API
void
fmp4_emsg_set_presentation_time(fmp4_emsg* emsg, uint64_t presentation_time);

FMP4_API
void
fmp4_emsg_set_event_duration(fmp4_emsg* emsg, uint32_t event_duration);

FMP4_API
void
fmp4_emsg_set_id(fmp4_emsg* emsg, uint32_t id);

FMP4_API
fmp4_result
fmp4_emsg_set_scheme_id_uri(fmp4_emsg* emsg, const char* scheme_id_uri);

FMP4_API
fmp4_result
fmp4_emsg_set_value(fmp4_emsg* emsg, const char* value);

FMP4_API
fmp4_result
fmp4_emsg_set_message(fmp4_emsg* emsg, const uint8_t* message_data, uint32_t message_size);


#ifdef __cplusplus
}
#endif

#endif

#ifdef MINIFMP4_IMPLEMENTATION
#include <math.h>
#include <stdlib.h>
#include <string.h>


FMP4_PRIVATE
uint32_t
fmp4_encode_sample_flags(const fmp4_sample_flags* flags);

FMP4_PRIVATE
void
fmp4_decode_sample_flags(uint32_t val, fmp4_sample_flags* flags);



FMP4_PRIVATE
void
fmp4_pack_uint64be(uint8_t* dest, uint64_t n);

FMP4_PRIVATE
void
fmp4_pack_uint32be(uint8_t* dest, uint32_t n);

FMP4_PRIVATE
void
fmp4_pack_uint24be(uint8_t* dest, uint32_t n);

FMP4_PRIVATE
void
fmp4_pack_uint16be(uint8_t* dest, uint16_t n);


#define BOX_ID(a, b, c, d) (((uint32_t)(a) << 24) | ((b) << 16) | ((c) << 8) | (d))

enum fmp4_box_id {
    BOX_ftyp = BOX_ID('f','t','y','p'),
    BOX_styp = BOX_ID('s','t','y','p'),
    BOX_moov = BOX_ID('m','o','o','v'),
    BOX_mvhd = BOX_ID('m','v','h','d'),
    BOX_trak = BOX_ID('t','r','a','k'),
    BOX_tkhd = BOX_ID('t','k','h','d'),
    BOX_mdia = BOX_ID('m','d','i','a'),
    BOX_mdhd = BOX_ID('m','d','h','d'),
    BOX_hdlr = BOX_ID('h','d','l','r'),
    BOX_minf = BOX_ID('m','i','n','f'),
    BOX_smhd = BOX_ID('s','m','h','d'),
    BOX_dinf = BOX_ID('d','i','n','f'),
    BOX_dref = BOX_ID('d','r','e','f'),
    BOX_url  = BOX_ID('u','r','l',' '),
    BOX_stbl = BOX_ID('s','t','b','l'),
    BOX_stsd = BOX_ID('s','t','s','d'),
    BOX_mp4a = BOX_ID('m','p','4','a'),
    BOX_alac = BOX_ID('a','l','a','c'),
    BOX_fLaC = BOX_ID('f','L','a','C'),
    BOX_esds = BOX_ID('e','s','d','s'),
    BOX_dfLa = BOX_ID('d','f','L','a'),
    BOX_stts = BOX_ID('s','t','t','s'),
    BOX_stsc = BOX_ID('s','t','s','c'),
    BOX_stsz = BOX_ID('s','t','s','z'),
    BOX_stco = BOX_ID('s','t','c','o'),
    BOX_mvex = BOX_ID('m','v','e','x'),
    BOX_mehd = BOX_ID('m','e','h','d'),
    BOX_trex = BOX_ID('t','r','e','x'),
    BOX_moof = BOX_ID('m','o','o','f'),
    BOX_mdat = BOX_ID('m','d','a','t'),
    BOX_mfhd = BOX_ID('m','f','h','d'),
    BOX_traf = BOX_ID('t','r','a','f'),
    BOX_tfhd = BOX_ID('t','f','h','d'),
    BOX_tfdt = BOX_ID('t','f','d','t'),
    BOX_trun = BOX_ID('t','r','u','n'),
    BOX_udta = BOX_ID('u','d','t','a'),
    BOX_ludt = BOX_ID('l','u','d','t'),
    BOX_tlou = BOX_ID('t','l','o','u'),
    BOX_alou = BOX_ID('a','l','o','u'),
    BOX_emsg = BOX_ID('e','m','s','g'),
    BOX_edts = BOX_ID('e','d','t','s'),
    BOX_elst = BOX_ID('e','l','s','t'),
    BOX_sgpd = BOX_ID('s','g','p','d'),
    BOX_sbgp = BOX_ID('s','b','g','p'),
    BOX_Opus = BOX_ID('O','p','u','s'),
    BOX_dOps = BOX_ID('d','O','p','s'),
    BOX_ac_3 = BOX_ID('a','c','-','3'),
    BOX_dac3 = BOX_ID('d','a','c','3'),
    BOX_ec_3 = BOX_ID('e','c','-','3'),
    BOX_dec3 = BOX_ID('d','e','c','3'),
};

typedef enum fmp4_box_id fmp4_box_id;

enum fmp4_hdlr_id {
    HDLR_soun = BOX_ID('s','o','u','n'),
};

typedef enum fmp4_hdlr_id fmp4_hdlr_id;

#undef BOX_ID

FMP4_PRIVATE
fmp4_result
fmp4_box_ftyp(fmp4_mux* mux);

FMP4_PRIVATE
fmp4_result
fmp4_box_styp(fmp4_mux* mux);

FMP4_PRIVATE
fmp4_result
fmp4_box_moov(fmp4_mux* mux);

FMP4_PRIVATE
fmp4_result
fmp4_box_trak(fmp4_mux* mux, const fmp4_track* track, uint32_t track_id);

FMP4_PRIVATE
fmp4_result
fmp4_box_trak_udta(fmp4_mux* mux, const fmp4_track* track);

FMP4_PRIVATE
fmp4_result
fmp4_box_loudness(fmp4_mux* mux, const fmp4_loudness* loudness);

FMP4_PRIVATE
fmp4_result
fmp4_box_emsg(fmp4_mux* mux, const fmp4_emsg* emsg);

FMP4_PRIVATE
fmp4_result
fmp4_box_moof(fmp4_mux* mux);

FMP4_PRIVATE
fmp4_result
fmp4_box_traf(fmp4_mux* mux, fmp4_track* track, uint32_t track_id);

FMP4_PRIVATE
fmp4_result
fmp4_box_mdat(fmp4_mux* mux);

FMP4_PRIVATE
fmp4_result
fmp4_box_begin(fmp4_mux* mux, fmp4_membuf* buffer, fmp4_box_id id);

FMP4_PRIVATE
fmp4_result
fmp4_box_end(fmp4_mux* mux, fmp4_membuf* buffer, fmp4_box_id id);

FMP4_PRIVATE
fmp4_result
fmp4_es_tag_begin(fmp4_mux* mux, fmp4_membuf* buffer, uint32_t tag_id);

FMP4_PRIVATE
fmp4_result
fmp4_es_tag_end(fmp4_mux* mux, fmp4_membuf* buffer, uint32_t tag_id);



FMP4_PRIVATE
void*
fmp4_default_malloc(size_t len, void* userdata);

FMP4_PRIVATE
void*
fmp4_default_realloc(void* ptr, size_t len, void* userdata);

FMP4_PRIVATE
void
fmp4_default_free(void* ptr, void* userdata);

FMP4_PRIVATE
const fmp4_allocator fmp4_default_allocator;



FMP4_PRIVATE
void
fmp4_membuf_init(fmp4_membuf* buf, const fmp4_allocator* allocator);

FMP4_PRIVATE
void
fmp4_membuf_reset(fmp4_membuf* buf);

FMP4_PRIVATE
void
fmp4_membuf_free(fmp4_membuf* buf);

FMP4_PRIVATE
fmp4_result
fmp4_membuf_ready(fmp4_membuf* buf, size_t len);

FMP4_PRIVATE
fmp4_result
fmp4_membuf_readyplus(fmp4_membuf* buf, size_t len);

/* appends data to the end of the buffer */
FMP4_PRIVATE
fmp4_result
fmp4_membuf_cat(fmp4_membuf* buf, const void* src, size_t len);

/* removes data from the end of the buffer */
FMP4_PRIVATE
fmp4_result
fmp4_membuf_uncat(fmp4_membuf* buf, void* dest, size_t len);

#define fmp4_membuf_push(buf, a) fmp4_membuf_cat(buf, &(a), sizeof(a))
#define fmp4_membuf_pop(buf, a) fmp4_membuf_uncat(buf, &(a), sizeof(a))


FMP4_PRIVATE
void*
fmp4_default_malloc(size_t len, void* userdata) {
    (void)userdata;
    return malloc(len);
}

FMP4_PRIVATE
void*
fmp4_default_realloc(void* ptr, size_t len, void* userdata) {
    (void)userdata;
    return realloc(ptr, len);
}

FMP4_PRIVATE
void
fmp4_default_free(void* ptr, void* userdata) {
    (void)userdata;
    free(ptr);
}

FMP4_PRIVATE
const fmp4_allocator fmp4_default_allocator = {
    NULL,
    fmp4_default_malloc,
    fmp4_default_realloc,
    fmp4_default_free
};

FMP4_PRIVATE
uint32_t
fmp4_encode_sample_flags(const fmp4_sample_flags* flags) {
    return 0
           | ( (flags->is_leading & 0x03) << 26)
           | ( (flags->depends_on & 0x03) << 24)
           | ( (flags->is_depended_on & 0x03) << 22)
           | ( (flags->has_redundancy & 0x03) << 20)
           | ( (flags->padding_value & 0x07) << 17)
           | ( (flags->is_non_sync & 0x01) << 16)
           |    flags->degradation_priority;
}

FMP4_PRIVATE
void
fmp4_decode_sample_flags(uint32_t val, fmp4_sample_flags* flags) {
    flags->degradation_priority = (val >>  0) & 0xFFFF;
    flags->is_non_sync          = (val >> 16) & 0x01;
    flags->padding_value        = (val >> 17) & 0x07;
    flags->has_redundancy       = (val >> 20) & 0x03;
    flags->is_depended_on       = (val >> 22) & 0x03;
    flags->depends_on           = (val >> 24) & 0x03;
    flags->is_leading           = (val >> 26) & 0x03;
}

FMP4_PRIVATE
void
fmp4_pack_uint64be(uint8_t* dest, uint64_t n) {
    dest[0] = ((n >> 56) & 0xFF );
    dest[1] = ((n >> 48) & 0xFF );
    dest[2] = ((n >> 40) & 0xFF );
    dest[3] = ((n >> 32) & 0xFF );
    dest[4] = ((n >> 24) & 0xFF );
    dest[5] = ((n >> 16) & 0xFF );
    dest[6] = ((n >>  8) & 0xFF );
    dest[7] = ((n >>  0) & 0xFF );
}

FMP4_PRIVATE
void
fmp4_pack_uint32be(uint8_t* dest, uint32_t n) {
    dest[0] = ((n >> 24) & 0xFF );
    dest[1] = ((n >> 16) & 0xFF );
    dest[2] = ((n >>  8) & 0xFF );
    dest[3] = ((n >>  0) & 0xFF );
}

FMP4_PRIVATE
void
fmp4_pack_uint24be(uint8_t* dest, uint32_t n) {
    dest[0] = ((n >> 16) & 0xFF );
    dest[1] = ((n >>  8) & 0xFF );
    dest[2] = ((n >>  0) & 0xFF );
}

FMP4_PRIVATE
void
fmp4_pack_uint16be(uint8_t* dest, uint16_t n) {
    dest[0] = ((n >>  8) & 0xFF );
    dest[1] = ((n >>  0) & 0xFF );
}




#ifndef FMP4_MEMBUF_BLOCKSIZE
#define FMP4_MEMBUF_BLOCKSIZE 4096
#endif

FMP4_PRIVATE
void
fmp4_membuf_init(fmp4_membuf* buf, const fmp4_allocator* allocator) {
    buf->x = NULL;
    buf->a = 0;
    buf->len = 0;
    buf->allocator = allocator;
}

FMP4_PRIVATE
void
fmp4_membuf_reset(fmp4_membuf* buf) {
    buf->len = 0;
}

FMP4_PRIVATE
void
fmp4_membuf_free(fmp4_membuf* buf) {
    if(buf->x != NULL) buf->allocator->free(buf->x, buf->allocator->userdata);
    buf->x = NULL;
    buf->a = 0;
    buf->len = 0;
}

FMP4_PRIVATE
fmp4_result
fmp4_membuf_ready(fmp4_membuf* buf, size_t len) {
    uint8_t* t;
    size_t a;
    if(len > buf->a) {
        a = (len + (FMP4_MEMBUF_BLOCKSIZE-1)) & -FMP4_MEMBUF_BLOCKSIZE;
        t = buf->allocator->realloc(buf->x, a, buf->allocator->userdata);
        if(t == NULL) return FMP4_OUTOFMEM;
        buf->x = t;
        buf->a = a;
    }
    return FMP4_OK;
}

FMP4_PRIVATE
fmp4_result
fmp4_membuf_readyplus(fmp4_membuf* buf, size_t len) {
    return fmp4_membuf_ready(buf, buf->len + len);
}

FMP4_PRIVATE
fmp4_result
fmp4_membuf_cat(fmp4_membuf* buf, const void* src, size_t len) {
    fmp4_result r;
    if( (r = fmp4_membuf_readyplus(buf, len)) != 0) return r;
    memcpy(&buf->x[buf->len], src, len);
    buf->len += len;
    return FMP4_OK;
}

FMP4_PRIVATE
fmp4_result
fmp4_membuf_uncat(fmp4_membuf* buf, void* dest, size_t len) {
    if(buf->len < len) return FMP4_MEMUNDERFLOW;
    memcpy(dest,&buf->x[buf->len - len],len);
    buf->len -= len;
    return FMP4_OK;
}

#undef FMP4_MEMBUF_BLOCKSIZE



#define BOX_ID(a, b, c, d) (((uint32_t)(a) << 24) | ((b) << 16) | ((c) << 8) | (d))

FMP4_PRIVATE
fmp4_result
fmp4_box_begin(fmp4_mux* mux, fmp4_membuf* buffer, fmp4_box_id box_id) {
    fmp4_result res;
    if( (res = fmp4_membuf_readyplus(buffer, sizeof(fmp4_box_id) + sizeof(buffer->len))) != FMP4_OK) return res;

    if( (res = fmp4_membuf_push(&mux->stack, buffer->len)) != FMP4_OK) return res;
    if( (res = fmp4_membuf_push(&mux->stack, box_id)) != FMP4_OK) return res;

    buffer->len += 4; /* leave space to write the size later */

    fmp4_pack_uint32be(&buffer->x[buffer->len], box_id);
    buffer->len += 4;

    return FMP4_OK;
}

FMP4_PRIVATE
fmp4_result
fmp4_box_end(fmp4_mux* mux, fmp4_membuf* buffer, fmp4_box_id box_id) {
    fmp4_result res;
    uint32_t id;
    size_t pos;

    if( (res = fmp4_membuf_pop(&mux->stack, id)) != FMP4_OK) return res;
    if(id != box_id) return FMP4_STACKERROR;

    if( (res = fmp4_membuf_pop(&mux->stack, pos)) != FMP4_OK) return res;
    if(buffer->len - pos > 0xFFFFFFFF) return FMP4_BOXTOOBIG;

    fmp4_pack_uint32be(&buffer->x[pos], (uint32_t) buffer->len - pos);
    return FMP4_OK;
}

FMP4_PRIVATE
fmp4_result
fmp4_es_tag_begin(fmp4_mux* mux, fmp4_membuf* buffer, uint32_t tag_id) {
    fmp4_result res;
    if( (res = fmp4_membuf_readyplus(buffer, sizeof(uint32_t) + sizeof(size_t))) != FMP4_OK) return res;

    buffer->x[buffer->len++] = (uint8_t)(tag_id & 0xFF);

    /* record our current position and tag */
    if( (res = fmp4_membuf_push(&mux->stack, buffer->len)) != FMP4_OK) return res;
    if( (res = fmp4_membuf_push(&mux->stack, tag_id)) != FMP4_OK) return res;

    buffer->len += 4; /* leave space to write the size later */

    return FMP4_OK;
}

FMP4_PRIVATE
fmp4_result
fmp4_es_tag_end(fmp4_mux* mux, fmp4_membuf* buffer, uint32_t tag_id) {
    fmp4_result res;
    uint32_t id;
    size_t pos;
    size_t len;

    if( (res = fmp4_membuf_pop(&mux->stack, id)) != FMP4_OK) return res;
    if(id != tag_id) return FMP4_STACKERROR;

    if( (res = fmp4_membuf_pop(&mux->stack, pos)) != FMP4_OK) return res;
    len = buffer->len - pos;
    if(len <= 4) return FMP4_STACKERROR;
    len -= 4; /* length of the size field is not included */

    if(len > 0x0FFFFFFF) return FMP4_ESTAGTOOBIG;

    buffer->x[pos++] = 0x80 | ((len >> 21) & 0x7F);
    buffer->x[pos++] = 0x80 | ((len >> 14) & 0x7F);
    buffer->x[pos++] = 0x80 | ((len >>  7) & 0x7F);
    buffer->x[pos++] = 0x00 | ((len >>  0) & 0x7F);

    return FMP4_OK;
}

#define WRITE_DATA(ptr,len) \
if((res = fmp4_membuf_cat(&mux->buffer,ptr,len)) != FMP4_OK) return res

#define WRITE_UINT64(val) \
if((res = fmp4_membuf_readyplus(&mux->buffer,8)) != FMP4_OK) return res;\
fmp4_pack_uint64be(&mux->buffer.x[mux->buffer.len], val); \
mux->buffer.len += 8

#define WRITE_UINT32(val) \
if((res = fmp4_membuf_readyplus(&mux->buffer,4)) != FMP4_OK) return res;\
fmp4_pack_uint32be(&mux->buffer.x[mux->buffer.len], val); \
mux->buffer.len += 4

#define WRITE_UINT24(val) \
if((res = fmp4_membuf_readyplus(&mux->buffer,3)) != FMP4_OK) return res;\
fmp4_pack_uint24be(&mux->buffer.x[mux->buffer.len], val); \
mux->buffer.len += 3

#define WRITE_UINT16(val) \
if((res = fmp4_membuf_readyplus(&mux->buffer,2)) != FMP4_OK) return res;\
fmp4_pack_uint16be(&mux->buffer.x[mux->buffer.len], val); \
mux->buffer.len += 2

#define WRITE_INT16(val) WRITE_UINT16(((uint16_t)val))

#define WRITE_UINT8(val) \
if((res = fmp4_membuf_readyplus(&mux->buffer,1)) != FMP4_OK) return res;\
mux->buffer.x[mux->buffer.len++] = (uint8_t)val

#define BOX_BEGIN(typ) \
if((res = fmp4_box_begin(mux,&(mux->buffer),typ)) != FMP4_OK) return res

#define BOX_BEGIN_FULL(typ, version, flags) \
if((res = fmp4_box_begin(mux,&(mux->buffer),typ)) != FMP4_OK) return res; \
WRITE_UINT32( (((uint32_t)version) << 24) | flags)

#define BOX_END(typ) \
if((res = fmp4_box_end(mux,&(mux->buffer),typ)) != FMP4_OK) return res

#define ES_TAG_BEGIN(typ) \
if((res = fmp4_es_tag_begin(mux,&(mux->buffer),typ)) != FMP4_OK) return res

#define ES_TAG_END(typ) \
if((res = fmp4_es_tag_end(mux,&(mux->buffer),typ)) != FMP4_OK) return res

FMP4_PRIVATE
fmp4_result
fmp4_box_ftyp(fmp4_mux* mux) {
    fmp4_result res;

    BOX_BEGIN(BOX_ftyp);
    {
        WRITE_DATA(mux->brand_major, 4); /* major brand */
        WRITE_UINT32(mux->brand_minor_version); /* major brand version */
        WRITE_DATA(mux->brand_major, 4); /* major brand, first in the list of compatible brands */
        if(mux->brands.len > 0) WRITE_DATA(mux->brands.x, mux->brands.len); /* compatible brands */
    }
    BOX_END(BOX_ftyp);

    return FMP4_OK;
}

FMP4_PRIVATE
fmp4_result
fmp4_box_styp(fmp4_mux* mux) {
    fmp4_result res;

    BOX_BEGIN(BOX_styp);
    {
        WRITE_DATA(mux->brand_major, 4); /* major brand */
        WRITE_UINT32(mux->brand_minor_version); /* major brand version */
        WRITE_DATA(mux->brand_major, 4); /* major brand, first in the list of compatible brands */
        if(mux->brands.len > 0) WRITE_DATA(mux->brands.x, mux->brands.len); /* compatible brands */
    }
    BOX_END(BOX_styp);

    return FMP4_OK;
}

FMP4_PRIVATE
fmp4_result
fmp4_box_moov(fmp4_mux* mux) {
    fmp4_result res;
    size_t i;
    size_t len;
    fmp4_track **tracks;

    tracks = (fmp4_track**)mux->tracks.x;
    i = 0;
    len = mux->tracks.len / sizeof(fmp4_track*);

    BOX_BEGIN(BOX_moov);
    {
        BOX_BEGIN_FULL(BOX_mvhd, 0, 0);
        {
            WRITE_UINT32(0); /* creation time */
            WRITE_UINT32(0); /* modification time */
            WRITE_UINT32(1000); /* timescale */
            WRITE_UINT32(0); /* duration */
            WRITE_UINT32(0x00010000); /* rate */
            WRITE_UINT16(0x0100); /* volume */
            WRITE_UINT16(0); /* reserved */
            WRITE_UINT32(0); /* reserved */
            WRITE_UINT32(0); /* reserved */

            /* matrix */
            WRITE_UINT32(0x00010000); WRITE_UINT32(0); WRITE_UINT32(0);
            WRITE_UINT32(0); WRITE_UINT32(0x00010000); WRITE_UINT32(0);
            WRITE_UINT32(0); WRITE_UINT32(0); WRITE_UINT32(0x40000000);

            /* pre_defined[6] */
            WRITE_UINT32(0); WRITE_UINT32(0); WRITE_UINT32(0);
            WRITE_UINT32(0); WRITE_UINT32(0); WRITE_UINT32(0);

            /* next track id */
            WRITE_UINT32(len+1);
        }
        BOX_END(BOX_mvhd);

        for(i=0;i<len;i++) {
            if( (res = fmp4_box_trak(mux,tracks[i],i+1)) != FMP4_OK) return res;
        }

        BOX_BEGIN(BOX_mvex);
        {
            BOX_BEGIN_FULL(BOX_mehd, 0, 0);
            {
                WRITE_UINT32(0); /* duration */
            }
            BOX_END(BOX_mehd);

            for(i=0;i<len;i++) {
            BOX_BEGIN_FULL(BOX_trex, 0, 0);
            {
                WRITE_UINT32(i+1);
                WRITE_UINT32(1); /* default sample description index */
                WRITE_UINT32(tracks[i]->default_sample_info.duration); /* default sample duration */
                WRITE_UINT32(tracks[i]->default_sample_info.size); /* default sample size */
                WRITE_UINT32(fmp4_encode_sample_flags(&tracks[i]->default_sample_info.flags));
            }
            BOX_END(BOX_trex);
            }
        }
        BOX_END(BOX_mvex);
    }
    BOX_END(BOX_moov);

    return FMP4_OK;
}

FMP4_PRIVATE
fmp4_result
fmp4_box_trak(fmp4_mux* mux, const fmp4_track* track, uint32_t id) {
    fmp4_result res;

    BOX_BEGIN(BOX_trak);
    {
        BOX_BEGIN_FULL(BOX_tkhd, 0, 0x07);
        {
            WRITE_UINT32(0); /* creation time */
            WRITE_UINT32(0); /* modification time */
            WRITE_UINT32(id);
            WRITE_UINT32(0); /* reserved */
            WRITE_UINT32(0); /* duration */

            /* reserved[2] */
            WRITE_UINT32(0); WRITE_UINT32(0);

            WRITE_UINT16(0); /* layer */
            WRITE_UINT16(0); /* alternate group */
            WRITE_UINT16(0x0100); /* volume */
            WRITE_UINT16(0); /* reserved */

            /* matrix */
            WRITE_UINT32(0x00010000); WRITE_UINT32(0); WRITE_UINT32(0);
            WRITE_UINT32(0); WRITE_UINT32(0x00010000); WRITE_UINT32(0);
            WRITE_UINT32(0); WRITE_UINT32(0); WRITE_UINT32(0x40000000);

            WRITE_UINT32(0); /* width */
            WRITE_UINT32(0); /* height */
        }
        BOX_END(BOX_tkhd);

        if(track->encoder_delay > 0) {
            BOX_BEGIN(BOX_edts);
            {
                BOX_BEGIN_FULL(BOX_elst, 0, 0);
                {
                    WRITE_UINT32(1); /* entry count */
                    WRITE_UINT32(0); /* segment duration */
                    WRITE_UINT32(track->encoder_delay); /* media_time */
                    WRITE_UINT16(1); /* media rate integer */
                    WRITE_UINT16(0); /* media rate fraction */
                }
                BOX_END(BOX_elst);
            }
            BOX_END(BOX_edts);
        }

        BOX_BEGIN(BOX_mdia);
        {
            BOX_BEGIN_FULL(BOX_mdhd, 0, 0);
            {
                WRITE_UINT32(0);
                WRITE_UINT32(0);
                WRITE_UINT32(track->time_scale);
                WRITE_UINT32(0);
                {
                uint16_t lang_code = ((track->language[0] & 31) << 10) | ((track->language[1] & 31) << 5) | (track->language[2] & 31);
                WRITE_UINT16(lang_code);
                }
                WRITE_UINT16(0);
            }
            BOX_END(BOX_mdhd);

            BOX_BEGIN_FULL(BOX_hdlr, 0, 0);
            {
                WRITE_UINT32(0); /* predefined */
                WRITE_UINT32(HDLR_soun); /* handler type */

                /* reserved[3] */
                WRITE_UINT32(0); WRITE_UINT32(0); WRITE_UINT32(0);
                WRITE_DATA("SoundHandler", 13);
            }
            BOX_END(BOX_hdlr);

            BOX_BEGIN(BOX_minf);
            {
                BOX_BEGIN_FULL(BOX_smhd, 0, 0);
                {
                    WRITE_UINT16(0); /* balance */
                    WRITE_UINT16(0); /* reserved */
                }
                BOX_END(BOX_smhd);

                BOX_BEGIN(BOX_dinf);
                {
                    BOX_BEGIN_FULL(BOX_dref, 0, 0);
                    {
                        WRITE_UINT32(1); /* entry count */
                        BOX_BEGIN_FULL(BOX_url, 0, 0x01);
                        BOX_END(BOX_url);
                    }
                    BOX_END(BOX_dref);
                }
                BOX_END(BOX_dinf);

                BOX_BEGIN(BOX_stbl);
                {
                    BOX_BEGIN_FULL(BOX_stsd, 0, 0);
                    {
                        WRITE_UINT32(1); /* entry count */
                        BOX_BEGIN((fmp4_box_id)track->codec);
                        {
                            /* sample entry */
                            WRITE_UINT32(0); WRITE_UINT16(0);
                            WRITE_UINT16(1); /* data reference index */

                            if(track->stream_type == FMP4_STREAM_TYPE_AUDIO) {
                                /* audio sample entry */
                                WRITE_UINT32(0); WRITE_UINT32(0); /* reserved */
                                WRITE_UINT16(track->info.audio.channels);
                                WRITE_UINT16(16);
                                WRITE_UINT32(0);
                                if(track->time_scale < 0x10000) {
                                    WRITE_UINT32((track->time_scale << 16));
                                } else {
                                    WRITE_UINT32(0);
                                }

                                if(track->codec == FMP4_CODEC_MP4A) {
                                    BOX_BEGIN_FULL(BOX_esds, 0, 0);
                                    {
                                        ES_TAG_BEGIN(0x03);
                                        {
                                            WRITE_UINT16(0); /* ES_ID */
                                            WRITE_UINT8(0); /* various ES flags */

                                            ES_TAG_BEGIN(0x04);
                                            {
                                                WRITE_UINT8(track->object_type); /* audio object type */
                                                WRITE_UINT8(track->stream_type << 2); /* stream type audiostream */
                                                WRITE_UINT24(track->info.audio.channels * 6144/8);
                                                WRITE_UINT32(0); /* maxbitrate */
                                                WRITE_UINT32(0); /* average bitrate bps */

                                                if(track->dsi.len > 0) {
                                                ES_TAG_BEGIN(0x05);
                                                {
                                                    WRITE_DATA(track->dsi.x, track->dsi.len);
                                                }
                                                ES_TAG_END(0x05);
                                                }
                                            }
                                            ES_TAG_END(0x04);

                                            ES_TAG_BEGIN(0x06);
                                            {
                                                WRITE_UINT8(2); /* predefined value, required */
                                            }
                                            ES_TAG_END(0x06);
                                        }
                                        ES_TAG_END(0x03);
                                    }
                                    BOX_END(BOX_esds);
                                } else if(track->codec == FMP4_CODEC_ALAC) {
                                    BOX_BEGIN_FULL(BOX_alac, 0, 0);
                                    WRITE_DATA(track->dsi.x, track->dsi.len);
                                    BOX_END(BOX_alac);
                                } else if(track->codec == FMP4_CODEC_FLAC) {
                                    BOX_BEGIN_FULL(BOX_dfLa, 0, 0);
                                    WRITE_DATA(track->dsi.x, track->dsi.len);
                                    BOX_END(BOX_dfLa);
                                } else if(track->codec == FMP4_CODEC_OPUS) {
                                    BOX_BEGIN(BOX_dOps);
                                    WRITE_DATA(track->dsi.x, track->dsi.len);
                                    BOX_END(BOX_dOps);
                                } else if(track->codec == FMP4_CODEC_AC3) {
                                    BOX_BEGIN(BOX_dac3);
                                    WRITE_DATA(track->dsi.x, track->dsi.len);
                                    BOX_END(BOX_dac3);
                                } else if(track->codec == FMP4_CODEC_EAC3) {
                                    BOX_BEGIN(BOX_dec3);
                                    WRITE_DATA(track->dsi.x, track->dsi.len);
                                    BOX_END(BOX_dec3);
                                }
                            }
                        }
                        BOX_END((fmp4_box_id)track->codec);

                    }
                    BOX_END(BOX_stsd);

                    BOX_BEGIN_FULL(BOX_stts, 0, 0);
                    {
                        WRITE_UINT32(0); /* sample cont */
                    }
                    BOX_END(BOX_stts);

                    BOX_BEGIN_FULL(BOX_stsc, 0, 0);
                    {
                        WRITE_UINT32(0); /* sample count */
                    }
                    BOX_END(BOX_stsc);

                    BOX_BEGIN_FULL(BOX_stsz, 0, 0);
                    {
                        WRITE_UINT32(0); /* sample size */
                        WRITE_UINT32(0); /* sample count */
                    }
                    BOX_END(BOX_stsz);

                    BOX_BEGIN_FULL(BOX_stco, 0, 0);
                    {
                        WRITE_UINT32(0); /* sample count */
                    }
                    BOX_END(BOX_stco);

                    if(track->roll_distance != 0) {
                        BOX_BEGIN_FULL(BOX_sgpd, 1, 0);
                        {
                            if(track->roll_type == FMP4_ROLL_TYPE_ROLL) {
                                WRITE_UINT32( BOX_ID('r','o','l','l') ); /* grouping type roll */
                            } else {
                                WRITE_UINT32( BOX_ID('p','r','o','l') ); /* grouping type prol */
                            }
                            WRITE_UINT32(2); /* default length */
                            WRITE_UINT32(1); /* entry count */
                            WRITE_INT16(track->roll_distance); /* roll distance */

                        }
                        BOX_END(BOX_sgpd);
                    }
                }
                BOX_END(BOX_stbl);
            }
            BOX_END(BOX_minf);
        }
        BOX_END(BOX_mdia);

        if((res = fmp4_box_trak_udta(mux,track)) != FMP4_OK) return res;
    }
    BOX_END(BOX_trak);
    return FMP4_OK;
}

FMP4_PRIVATE
fmp4_result
fmp4_box_trak_udta(fmp4_mux* mux, const fmp4_track* track) {
    fmp4_result res;

    size_t i;
    size_t len;

    fmp4_loudness** loudnesses;

    if(track->loudness.len == 0) return FMP4_OK; /* no userdata */

    loudnesses = (fmp4_loudness **)track->loudness.x;
    len = track->loudness.len / sizeof(fmp4_loudness*);

    BOX_BEGIN(BOX_udta);
    {
        BOX_BEGIN(BOX_ludt);
        {
            /* we're going to do 2 loops which is kinda lame,
             * first for the track loudnesses, then album */
            for(i=0;i<len;i++) {
              if(loudnesses[i]->type == FMP4_LOUDNESS_TRACK) {
                  if( (res = fmp4_box_loudness(mux,loudnesses[i])) != FMP4_OK) return res;
              }
            }
            for(i=0;i<len;i++) {
              if(loudnesses[i]->type == FMP4_LOUDNESS_ALBUM) {
                  if( (res = fmp4_box_loudness(mux,loudnesses[i])) != FMP4_OK) return res;
              }
            }
        }
        BOX_END(BOX_ludt);
    }
    BOX_END(BOX_udta);

    return FMP4_OK;
}

FMP4_PRIVATE
fmp4_result
fmp4_box_loudness(fmp4_mux* mux, const fmp4_loudness* loudness) {
    fmp4_result res;
    size_t i;
    size_t len;
    uint32_t boxtype;

    fmp4_measurement** measurements = (fmp4_measurement**)loudness->measurements.x;
    len = (loudness->measurements.len / sizeof(fmp4_measurement*)) & 0xFF;

    if(loudness->type == FMP4_LOUDNESS_TRACK) {
        boxtype = BOX_tlou;
    } else {
        boxtype = BOX_alou;
    }

    BOX_BEGIN_FULL(boxtype,0, 0);
    {
        /* reserved(3), downmix_ID(7), DRC_set_ID(6) */
        uint16_t rdD = 0 |
          (loudness->downmix_id << 6) |
          (loudness->drc_id & 0x3F);

        /* bs_sample_peak_level(12), bs_true_peak_level(12), */
        /* measurement_system_for_TP (4), reliability_for_TP(4) */
        uint32_t bbmr = 0 |
          ((uint32_t)loudness->sample_peak << (12 + 4 + 4) & 0xFFF00000) |
          ((uint32_t)loudness->true_peak   << (     4 + 4) & 0x000FFF00) |
          (loudness->system                << (         4) & 0x000000F0) |
          (loudness->reliability                           & 0x0000000F);

        WRITE_UINT16(rdD);
        WRITE_UINT32(bbmr);

        WRITE_UINT8((uint8_t)len);
        for(i=0;i<len;i++) {
            WRITE_UINT8(measurements[i]->method);
            WRITE_UINT8(measurements[i]->value);
            WRITE_UINT8((measurements[i]->system << 4) | measurements[i]->reliability);
        }
    }
    BOX_END(boxtype);
    return FMP4_OK;
}


FMP4_PRIVATE
fmp4_result
fmp4_box_emsg(fmp4_mux* mux, const fmp4_emsg* emsg) {
    fmp4_result res;

    BOX_BEGIN_FULL(BOX_emsg, emsg->version, 0);
    {
        if(emsg->version == 0) {
            WRITE_DATA(emsg->scheme_id_uri.x,emsg->scheme_id_uri.len);
            WRITE_DATA(emsg->value.x,emsg->value.len);
            WRITE_UINT32(emsg->timescale);
            WRITE_UINT32(emsg->presentation_time_delta);
            WRITE_UINT32(emsg->event_duration);
            WRITE_UINT32(emsg->id);
        } else if(emsg->version == 1) {
            WRITE_UINT32(emsg->timescale);
            WRITE_UINT64(emsg->presentation_time);
            WRITE_UINT32(emsg->event_duration);
            WRITE_UINT32(emsg->id);
            WRITE_DATA(emsg->scheme_id_uri.x,emsg->scheme_id_uri.len);
            WRITE_DATA(emsg->value.x,emsg->value.len);
        }
        WRITE_DATA(emsg->message.x,emsg->message.len);
    }
    BOX_END(BOX_emsg);

    return FMP4_OK;
}

FMP4_PRIVATE
fmp4_result
fmp4_box_moof(fmp4_mux* mux) {
    fmp4_result res;
    size_t i = 0;
    size_t len = mux->tracks.len / sizeof(fmp4_track*);
    fmp4_track** tracks = (fmp4_track**)mux->tracks.x;

    mux->moof_offset = mux->buffer.len;

    BOX_BEGIN(BOX_moof);
    {
        BOX_BEGIN_FULL(BOX_mfhd, 0, 0);
        {
            WRITE_UINT32(++mux->fragments);
        }
        BOX_END(BOX_mfhd);

        for(i=0;i<len;i++) {
            if( (res = fmp4_box_traf(mux, tracks[i], i+1)) != FMP4_OK) return res;
        }
    }
    BOX_END(BOX_moof);

    return FMP4_OK;
}

FMP4_PRIVATE
fmp4_result
fmp4_box_traf(fmp4_mux* mux, fmp4_track* track, uint32_t id) {
    fmp4_result res;

    BOX_BEGIN(BOX_traf);
    {
        uint32_t tfhd_flags = 0;
        uint32_t trun_flags = 0;
        uint32_t default_flags = fmp4_encode_sample_flags(&track->default_sample_info.flags);

        tfhd_flags |= 0x020000; /* default base is moof */
        trun_flags |= 0x01; /* data offset present */

        if(track->trun_sample_duration_set) {
            if(track->trun_sample_duration != track->default_sample_info.duration) {
                tfhd_flags |= 0x08; /* default duration present */
            }
        } else {
            /* we had different track durations so include all of them */
            trun_flags |= 0x100; /* sample duration present */
        }

        if(track->trun_sample_size_set) {
            if(track->trun_sample_size != track->default_sample_info.size) {
                tfhd_flags |= 0x10; /* default size present */
            }
        } else {
            /* we had different track sizes so include all of them */
            trun_flags |= 0x200; /* sample size present */
        }

        if(track->trun_sample_flags_set) {
            if(track->trun_sample_flags != default_flags) {
                tfhd_flags |= 0x20; /* default sample flags present */
                /* if our first sample flags match the rest of the trun we won't need
                 * them */
                if(track->first_sample_flags != track->trun_sample_flags) {
                    trun_flags |= 0x04; /* first-sample-flags present */
                }
            } else {
                /* we can inherit track sample flags, check if we need the first
                 * sample flags */
                if(track->first_sample_flags != default_flags) {
                    trun_flags |= 0x04; /* first-sample-flags present */
                }
            }
        } else {
            /* we had differing sample flags so just include all of them */
            trun_flags |= 0x400; /* sample flags present */
        }


        BOX_BEGIN_FULL(BOX_tfhd, 0, tfhd_flags);
        {
            WRITE_UINT32(id);
            if(tfhd_flags & 0x08) {
                WRITE_UINT32(track->trun_sample_duration);
            }
            if(tfhd_flags & 0x10) {
                WRITE_UINT32(track->trun_sample_size);
            }
            if(tfhd_flags & 0x20) {
                WRITE_UINT32(track->trun_sample_flags);
            }
        }
        BOX_END(BOX_tfhd);

        BOX_BEGIN_FULL(BOX_tfdt, 1, 0);
        {
            WRITE_UINT64(track->base_media_decode_time);
        }
        BOX_END(BOX_tfdt);
        track->base_media_decode_time += (uint64_t) track->trun_sample_count;

        BOX_BEGIN_FULL(BOX_trun, 0, trun_flags);
        {
            size_t i;
            size_t len = track->sample_info.len / sizeof(fmp4_sample_info);
            fmp4_sample_info* info = (fmp4_sample_info *)track->sample_info.x;

            WRITE_UINT32((uint32_t)len); /* sample count */
            track->data_offset_pos = mux->buffer.len; /* we'll use this later to write the mdat
                                                         data offset */
            WRITE_UINT32(0); /* filler for data offset */
            if(trun_flags & 0x04) { WRITE_UINT32(track->first_sample_flags); }
            for(i=0;i<len;i++) {
                if(trun_flags & 0x100) {
                    WRITE_UINT32(info[i].duration);
                }
                if(trun_flags & 0x200) {
                    WRITE_UINT32(info[i].size);
                }
                if(trun_flags & 0x400) {
                    WRITE_UINT32(fmp4_encode_sample_flags(&info[i].flags));
                }
            }
        }
        BOX_END(BOX_trun);

        if(track->roll_distance != 0) {
            BOX_BEGIN_FULL(BOX_sbgp, 0, 0);
            {
                size_t i;
                size_t len = track->sample_info.len / sizeof(fmp4_sample_info);
                fmp4_sample_info* info = (fmp4_sample_info *)track->sample_info.x;
                uint32_t last_sample_group = 0xffffffff; /* set to invalid value to trigger first entry */
                size_t entrycount_pos = 0; /* position to write final number of entries */
                size_t entrycount = 0;
                size_t samplecount_pos = 0; /* position to write the sample count for the current entry */
                size_t samplecount = 0;

                if(track->roll_type == FMP4_ROLL_TYPE_ROLL) {
                    WRITE_UINT32( BOX_ID('r','o','l','l') ); /* grouping type roll */
                } else {
                    WRITE_UINT32( BOX_ID('p','r','o','l') ); /* grouping type prol */
                }

                /* iterate through samples and get assigned into correct sample groups */
                entrycount_pos = mux->buffer.len; /* entry count, will update later */
                WRITE_UINT32(0); /* reserve space for the entry count */

                for(i=0;i<len;i++) {
                    if(info[i].sample_group != last_sample_group) {
                        if(i > 0) fmp4_pack_uint32be(&mux->buffer.x[samplecount_pos],samplecount);

                        ++entrycount;
                        last_sample_group = info[i].sample_group;
                        samplecount_pos = mux->buffer.len;
                        samplecount = 1;
                        WRITE_UINT32(0); /* reserve space for the samplecount */
                        WRITE_UINT32(last_sample_group); /* group description index, 0 = sync sample, 1 = non-sync sample */
                    } else {
                        ++samplecount;
                    }
                }

                fmp4_pack_uint32be(&mux->buffer.x[samplecount_pos],samplecount);
                fmp4_pack_uint32be(&mux->buffer.x[entrycount_pos],entrycount);
            }
            BOX_END(BOX_sbgp);
        }
    }
    BOX_END(BOX_traf);
    return FMP4_OK;
}

FMP4_PRIVATE
fmp4_result
fmp4_box_mdat(fmp4_mux* mux) {
    fmp4_result res;
    size_t i = 0;
    size_t len = mux->tracks.len / sizeof(fmp4_track*);
    fmp4_track** tracks = (fmp4_track**)mux->tracks.x;

    BOX_BEGIN(BOX_mdat);
    {
        for(i=0;i<len;i++) {
            fmp4_pack_uint32be(&mux->buffer.x[tracks[i]->data_offset_pos], mux->buffer.len - mux->moof_offset);
            WRITE_DATA(tracks[i]->mdat.x, tracks[i]->mdat.len);
        }
    }
    BOX_END(BOX_mdat);

    return FMP4_OK;
}

#undef WRITE_DATA
#undef WRITE_UINT64
#undef WRITE_UINT32
#undef WRITE_UINT24
#undef WRITE_UINT16
#undef WRITE_UINT8
#undef BOX_BEGIN
#undef BOX_BEGIN_FULL
#undef BOX_END
#undef ES_TAG_BEGIN
#undef ES_TAG_END
#undef BOX_ID



FMP4_API
fmp4_track*
fmp4_mux_new_track(fmp4_mux* mux) {
    fmp4_track* track = fmp4_track_new(mux->allocator);
    if(track == NULL) return track;

    if(fmp4_membuf_cat(&mux->alloc_track,&track,sizeof(fmp4_track*)) != FMP4_OK) {
        fmp4_track_free(track);
        return NULL;
    }

    if(fmp4_mux_add_track(mux,track) != FMP4_OK) {
        fmp4_membuf_uncat(&mux->alloc_track, &track, sizeof(fmp4_track*));
        fmp4_track_free(track);
        return NULL;
    }

    return track;
}

FMP4_API
fmp4_emsg*
fmp4_mux_new_emsg(fmp4_mux* mux) {
    fmp4_emsg* emsg = fmp4_emsg_new(mux->allocator);
    if(emsg == NULL) return emsg;

    if(fmp4_membuf_cat(&mux->alloc_emsg,&emsg,sizeof(fmp4_emsg*)) != FMP4_OK) {
        fmp4_emsg_free(emsg);
        return NULL;
    }

    return emsg;
}

FMP4_API
fmp4_result
fmp4_mux_add_track(fmp4_mux* mux, const fmp4_track* track) {
    return fmp4_membuf_cat(&mux->tracks,&track,sizeof(fmp4_track*));
}

FMP4_API
fmp4_result
fmp4_mux_add_brand(fmp4_mux* mux, const char brand[4]) {
    return fmp4_membuf_cat(&mux->brands, brand, 4);
}

FMP4_API
void
fmp4_mux_set_brand_major(fmp4_mux* mux, const char brand[4], uint32_t ver) {
    memcpy(mux->brand_major, brand, 4);
    mux->brand_minor_version = ver;
    return;
}

FMP4_API
fmp4_result
fmp4_mux_write_init(fmp4_mux* mux, fmp4_write_cb write, void* userdata) {
    fmp4_result res;

    if( (res = fmp4_mux_validate_init(mux)) != FMP4_OK) goto cleanup;

    if( (res = fmp4_box_ftyp(mux)) != FMP4_OK) goto cleanup;
    if( (res = fmp4_box_moov(mux)) != FMP4_OK) goto cleanup;

    if(write(mux->buffer.x,mux->buffer.len,userdata) != mux->buffer.len) {
        res = FMP4_WRITEERR;
        goto cleanup;
    }

    res = FMP4_OK;

    cleanup:

    fmp4_membuf_reset(&mux->buffer);
    fmp4_membuf_reset(&mux->stack);

    return FMP4_OK;
}

FMP4_API
fmp4_result
fmp4_mux_write_segment(fmp4_mux* mux, fmp4_write_cb write, void* userdata) {
    fmp4_result res;
    size_t i;
    size_t len;
    fmp4_emsg** emsgs;
    fmp4_track** tracks;

    tracks = (fmp4_track**)mux->tracks.x;
    emsgs = (fmp4_emsg**)mux->emsgs.x;

    if( (res = fmp4_mux_validate_segment(mux)) != FMP4_OK) goto cleanup;

    if( (res = fmp4_box_styp(mux)) != FMP4_OK) goto cleanup;

    len = mux->emsgs.len / sizeof(fmp4_emsg*);
    for(i=0;i<len;i++) {
        if( (res = fmp4_box_emsg(mux, emsgs[i])) != FMP4_OK) goto cleanup;
    }

    if( (res = fmp4_box_moof(mux)) != FMP4_OK) goto cleanup;
    if( (res = fmp4_box_mdat(mux)) != FMP4_OK) goto cleanup;

    if(write(mux->buffer.x,mux->buffer.len,userdata) != mux->buffer.len) {
        res = FMP4_WRITEERR;
        goto cleanup;
    }

    res = FMP4_OK;

    cleanup:

    len = mux->tracks.len / sizeof(fmp4_track*);
    for(i=0;i<len;i++) {
        fmp4_membuf_reset(&tracks[i]->mdat);
        fmp4_membuf_reset(&tracks[i]->sample_info);
    }

    fmp4_membuf_reset(&mux->buffer);
    fmp4_membuf_reset(&mux->stack);
    fmp4_membuf_reset(&mux->emsgs);

    return res;
}

FMP4_API
fmp4_result
fmp4_mux_add_emsg(fmp4_mux* mux, const fmp4_emsg* emsg) {
    return fmp4_membuf_cat(&mux->emsgs,&emsg,sizeof(fmp4_emsg*));
}

FMP4_API
fmp4_result
fmp4_mux_validate_init(const fmp4_mux* mux) {
    fmp4_result res;
    fmp4_track** tracks;
    size_t i;
    size_t len;

    if(mux->tracks.len == 0) return FMP4_NOTRACKS;

    tracks = (fmp4_track**)mux->tracks.x;
    len = mux->tracks.len / sizeof(fmp4_track*);

    for(i=0;i<len;i++) {
        if( (res = fmp4_track_validate_init(tracks[i])) != FMP4_OK) return res;
    }

    return FMP4_OK;
}

FMP4_API
fmp4_result
fmp4_mux_validate_segment(const fmp4_mux* mux) {
    fmp4_result res;
    fmp4_track** tracks;
    fmp4_emsg** emsgs;
    size_t i;
    size_t len;
    size_t totalsamples;

    tracks = (fmp4_track**)mux->tracks.x;
    len = mux->tracks.len / sizeof(fmp4_track*);
    totalsamples = 0;

    for(i=0;i<len;i++) {
        totalsamples += tracks[i]->sample_info.len / sizeof(fmp4_sample_info);
        res = fmp4_track_validate_segment(tracks[i]);
        switch(res) {
            case FMP4_OK: /* fall-through */
            case FMP4_NOSAMPLES: break;
            default: return res;
        }
    }
    if(totalsamples == 0) return FMP4_NOSAMPLES;

    emsgs = (fmp4_emsg**)mux->emsgs.x;
    len = mux->emsgs.len / sizeof(fmp4_emsg*);
    for(i=0;i<len;i++) {
        if( (res = fmp4_emsg_validate(emsgs[i])) != FMP4_OK) return res;
    }

    return FMP4_OK;
}



FMP4_API
fmp4_loudness*
fmp4_track_new_loudness(fmp4_track* track) {
    fmp4_loudness* loudness = fmp4_loudness_new(track->allocator);
    if(loudness == NULL) return loudness;

    if(fmp4_membuf_cat(&track->alloc_loudness,&loudness,sizeof(fmp4_loudness*)) != FMP4_OK) {
        fmp4_loudness_free(loudness);
        return NULL;
    }

    if(fmp4_track_add_loudness(track,loudness) != FMP4_OK) {
        fmp4_loudness_free(loudness);
        fmp4_membuf_uncat(&track->alloc_loudness, &loudness, sizeof(fmp4_loudness*));
        return NULL;
    }

    return loudness;
}

FMP4_API
fmp4_result
fmp4_track_add_sample(fmp4_track* track, const void* data, const fmp4_sample_info* info) {
    fmp4_result res;

    if((res = fmp4_membuf_cat(&track->sample_info,info,sizeof(fmp4_sample_info))) != FMP4_OK) return res;
    if((res = fmp4_membuf_cat(&track->mdat,data,info->size)) != FMP4_OK) return res;

    if(track->sample_info.len == sizeof(fmp4_sample_info)) {
        /* this is the first sample, always record the first sample's flags */
        track->first_sample_flags = fmp4_encode_sample_flags(&info->flags);
        track->trun_sample_flags_set = 0;

        /* set the duration flag, used to see if all samples use the same duration */
        track->trun_sample_duration_set = 1;
        track->trun_sample_duration = info->duration;

        /* set the size flag, used to see if all samples use the same duration */
        track->trun_sample_size_set = 1;
        track->trun_sample_size = info->size;

        /* lastly reset the sample count */
        track->trun_sample_count       = 0;
    } else {
        if(track->sample_info.len == (sizeof(fmp4_sample_info) * 2)) {
            /* second sample - set the sample_flags, similar to duration */
            track->trun_sample_flags_set = 1;
            track->trun_sample_flags = fmp4_encode_sample_flags(&info->flags);
        } else {
            /* third+ sample - where we check that all sample flags matched */
            if(track->trun_sample_flags_set == 1) {
                if(track->trun_sample_flags != fmp4_encode_sample_flags(&info->flags)) {
                    /* flags mis-match, we'll need to report all flags in the
                     * track fragment header */
                    track->trun_sample_flags_set = 0;
                }
            }
        }

        /* on all samples - check the duration */
        if(track->trun_sample_duration_set == 1) {
            if(track->trun_sample_duration != info->duration) {
                /* we had a mis-match occur, we'll need to
                 * report all durations in the track header */
                track->trun_sample_duration_set = 0;
            }
        }

        /* on all samples - check the size */
        if(track->trun_sample_size_set == 1) {
            if(track->trun_sample_size != info->size) {
                /* we had a mis-match occur, we'll need to
                 * report all sizes in the track header */
                track->trun_sample_size_set = 0;
            }
        }
    }

    track->trun_sample_count += (uint64_t)info->duration;
    return FMP4_OK;
}

FMP4_API
fmp4_result
fmp4_track_add_loudness(fmp4_track* track, const fmp4_loudness* loudness) {
    return fmp4_membuf_cat(&track->loudness, &loudness, sizeof(fmp4_loudness*));
}


/* validates the track has valid init-related data */
FMP4_API
fmp4_result
fmp4_track_validate_init(const fmp4_track* track) {
    fmp4_result res;
    size_t i;
    size_t len;
    fmp4_loudness** loudnesses;

    switch(track->stream_type) {
        case FMP4_STREAM_TYPE_FORBIDDEN: return FMP4_STREAMINVALID;
        case FMP4_STREAM_TYPE_AUDIO: {
            if(track->info.audio.channels == 0) return FMP4_CHANNELSINVALID;
            switch(track->codec) {
                case FMP4_CODEC_UNDEFINED: return FMP4_CODECINVALID;
                case FMP4_CODEC_MP4A:
                    if(track->object_type == FMP4_OBJECT_TYPE_FORBIDDEN) return FMP4_OBJECTINVALID;
                    if(track->object_type == FMP4_OBJECT_TYPE_MP3) break; /* MP3 does not have DSI */
                    /* fall-through */
                case FMP4_CODEC_ALAC: /* fall-through */
                case FMP4_CODEC_FLAC: /* fall-through */
                case FMP4_CODEC_OPUS: /* fall-through */
                default: {
                    if(track->dsi.len == 0) return FMP4_MISSINGDSI;
                    break;
                }
            }
            break;
        }
        default: break;
    }
    if(track->time_scale == 0) return FMP4_TIMESCALEINVALID;

    loudnesses = (fmp4_loudness**)track->loudness.x;
    len = track->loudness.len / sizeof(fmp4_loudness*);
    for(i=0;i<len;i++) {
        if( (res = fmp4_loudness_validate(loudnesses[i])) != FMP4_OK) return res;
    }

    return FMP4_OK;
}

/* validates the track has valid media-related data */
FMP4_API
fmp4_result
fmp4_track_validate_segment(const fmp4_track* track) {
    if(track->sample_info.len == 0) return FMP4_NOSAMPLES;
    return FMP4_OK;
}


FMP4_API
fmp4_measurement*
fmp4_loudness_new_measurement(fmp4_loudness* loudness) {
    fmp4_measurement* measurement = fmp4_measurement_new(loudness->allocator);
    if(measurement == NULL) return measurement;

    if(fmp4_membuf_cat(&loudness->alloc_measurement,&measurement,sizeof(fmp4_measurement*)) != FMP4_OK) {
        fmp4_measurement_free(measurement);
        return NULL;
    }

    if(fmp4_loudness_add_measurement(loudness,measurement) != FMP4_OK) {
        fmp4_measurement_free(measurement);
        fmp4_membuf_uncat(&loudness->alloc_measurement, &measurement, sizeof(fmp4_measurement*));
        return NULL;
    }

    return measurement;
}


FMP4_API
fmp4_result
fmp4_loudness_add_measurement(fmp4_loudness* loudness, const fmp4_measurement* measurement) {
    return fmp4_membuf_cat(&loudness->measurements, &measurement, sizeof(fmp4_measurement*));
}


FMP4_API
fmp4_result
fmp4_loudness_validate(const fmp4_loudness* loudness) {
    fmp4_result res;
    fmp4_measurement** measurements;
    size_t i;
    size_t len;
    if(loudness->type == FMP4_LOUDNESS_UNDEF) return FMP4_LOUDNESSNOTSET;
    if(loudness->sample_peak == 0 && loudness->true_peak == 0) return FMP4_LOUDNESSNOTSET;
    if(loudness->true_peak != 0) {
        if(loudness->system > 5) return FMP4_SYSTEMINVALID;
        if(loudness->reliability > 3) return FMP4_RELIABILITYINVALID;
    }

    measurements = (fmp4_measurement**)loudness->measurements.x;
    len = loudness->measurements.len / sizeof(fmp4_measurement*);
    for(i=0;i<len;i++) {
        if( (res = fmp4_measurement_validate(measurements[i])) != FMP4_OK) return res;
    }

    return FMP4_OK;
}


FMP4_API
fmp4_result
fmp4_measurement_validate(const fmp4_measurement* measurement) {
    if(measurement->method == 0)      return FMP4_METHODINVALID;
    if(measurement->system == 0)      return FMP4_SYSTEMINVALID;
    if(measurement->reliability == 0) return FMP4_RELIABILITYINVALID;
    return FMP4_OK;
}


FMP4_API
fmp4_result
fmp4_emsg_validate(const fmp4_emsg* emsg) {
    if(emsg->version > 1) return FMP4_INVALIDEMSGVER;
    if(emsg->timescale == 0) return FMP4_TIMESCALEINVALID;
    if(emsg->scheme_id_uri.len == 0) return FMP4_EMSGSCHEMENOTSET;
    if(emsg->value.len == 0) return FMP4_EMSGVALUENOTSET;
    if(emsg->message.len == 0) return FMP4_EMSGMESSAGENOTSET;

    return FMP4_OK;
}


FMP4_API
fmp4_result
fmp4_mux_get_brand_major(const fmp4_mux* mux, char brand[4], uint32_t* ver) {
    memcpy(brand,mux->brand_major,sizeof(char)*4);
    if(ver != NULL) *ver = mux->brand_minor_version;
    return FMP4_OK;
}

FMP4_API
uint32_t
fmp4_mux_get_brand_minor_version(const fmp4_mux* mux) {
    return mux->brand_minor_version;
}

FMP4_API
const char*
fmp4_mux_get_brands(const fmp4_mux* mux, size_t* len) {
    if(mux->brands.len == 0) return NULL;
    *len = mux->brands.len / sizeof(char);
    return (const char *)mux->brands.x;
}

FMP4_API
size_t
fmp4_mux_get_track_count(const fmp4_mux* mux) {
    return mux->tracks.len / sizeof(fmp4_track*);
}

FMP4_API
fmp4_track**
fmp4_mux_get_tracks(const fmp4_mux* mux, size_t* count) {
    if(count != NULL) *count = mux->tracks.len / sizeof(fmp4_track*);
    return (fmp4_track**)mux->tracks.x;
}

FMP4_API
fmp4_track*
fmp4_mux_get_track(const fmp4_mux* mux, size_t index) {
    size_t len = mux->tracks.len / sizeof(fmp4_track*);
    fmp4_track** tracks = (fmp4_track**)mux->tracks.x;
    if(index < len) return tracks[index];
    return NULL;
}

FMP4_API
fmp4_stream_type
fmp4_track_get_stream_type(const fmp4_track* track) {
    return track->stream_type;
}

FMP4_API
fmp4_codec
fmp4_track_get_codec(const fmp4_track* track) {
    return track->codec;
}

FMP4_API
fmp4_object_type
fmp4_track_get_object_type(const fmp4_track* track) {
    return track->object_type;
}

FMP4_API
uint64_t
fmp4_track_get_base_media_decode_time(const fmp4_track* track) {
    return track->base_media_decode_time;
}

FMP4_API
uint32_t
fmp4_track_get_time_scale(const fmp4_track* track) {
    return track->time_scale;
}

FMP4_API
const char*
fmp4_track_get_language(const fmp4_track* track) {
    if(track->language[0] == '\0') return NULL;
    return (const char *)track->language;
}

FMP4_API
uint16_t
fmp4_track_get_audio_channels(const fmp4_track* track) {
    return track->info.audio.channels;
}

FMP4_API
uint32_t
fmp4_track_get_encoder_delay(const fmp4_track* track) {
    return track->encoder_delay;
}

FMP4_API
int16_t
fmp4_track_get_roll_distance(const fmp4_track* track) {
    return track->roll_distance;
}

FMP4_API
fmp4_roll_type
fmp4_track_get_roll_type(const fmp4_track* track) {
    return track->roll_type;
}

FMP4_API
fmp4_result
fmp4_track_get_default_sample_info(const fmp4_track *track, fmp4_sample_info* info) {
    memcpy(info,&track->default_sample_info,sizeof(fmp4_sample_info));
    return FMP4_OK;
}

FMP4_API
size_t
fmp4_track_get_loudness_count(const fmp4_track* track) {
    return track->loudness.len / sizeof(fmp4_loudness*);
}

FMP4_API
fmp4_loudness*
fmp4_track_get_loudness(const fmp4_track* track, size_t index) {
    size_t count = track->loudness.len / sizeof(fmp4_loudness*);
    fmp4_loudness** loudnesses = (fmp4_loudness**)track->loudness.x;
    if(index < count) return loudnesses[index];
    return NULL;
}

FMP4_API
fmp4_loudness**
fmp4_track_get_loudnesses(const fmp4_track* track, size_t* count) {
    if(count != NULL) *count = track->loudness.len / sizeof(fmp4_loudness*);
    return (fmp4_loudness**)track->loudness.x;
}

FMP4_API
size_t
fmp4_track_get_sample_info_count(const fmp4_track* track) {
    return track->sample_info.len / sizeof(fmp4_sample_info*);
}

FMP4_API
fmp4_sample_info*
fmp4_track_get_sample_info(const fmp4_track* track, size_t index) {
    size_t count = track->sample_info.len / sizeof(fmp4_sample_info*);
    fmp4_sample_info** sample_infoes = (fmp4_sample_info**)track->sample_info.x;
    if(index < count) return sample_infoes[index];
    return NULL;
}

FMP4_API
fmp4_sample_info**
fmp4_track_get_sample_infos(const fmp4_track* track, size_t* count) {
    if(count != NULL) *count = track->sample_info.len / sizeof(fmp4_sample_info*);
    return (fmp4_sample_info**)track->sample_info.x;
}

FMP4_API
const void*
fmp4_track_get_dsi(const fmp4_track* track, size_t* len) {
    if(len != NULL) *len = track->dsi.len;
    return track->dsi.x;
}

FMP4_API
fmp4_result
fmp4_track_get_first_sample_flags(const fmp4_track* track, fmp4_sample_flags* flags) {
    fmp4_decode_sample_flags(track->first_sample_flags, flags);
    return FMP4_OK;
}

FMP4_API
uint8_t
fmp4_track_get_trun_sample_flags_set(const fmp4_track* track) {
    return track->trun_sample_flags_set;
}

FMP4_API
fmp4_result
fmp4_track_get_trun_sample_flags(const fmp4_track* track, fmp4_sample_flags* flags) {
    fmp4_decode_sample_flags(track->trun_sample_flags, flags);
    return FMP4_OK;
}

FMP4_API
uint8_t
fmp4_track_get_trun_sample_duration_set(const fmp4_track* track) {
    return track->trun_sample_duration_set;
}

FMP4_API
uint32_t
fmp4_track_get_trun_sample_duration(const fmp4_track* track) {
    return track->trun_sample_duration;
}

FMP4_API
uint8_t
fmp4_track_get_trun_sample_size_set(const fmp4_track* track) {
    return track->trun_sample_size_set;
}

FMP4_API
uint32_t
fmp4_track_get_trun_sample_size(const fmp4_track* track) {
    return track->trun_sample_size;
}

FMP4_API
uint64_t
fmp4_track_get_trun_sample_count(const fmp4_track* track) {
    return track->trun_sample_count;
}

FMP4_API
fmp4_loudness_type
fmp4_loudness_get_type(const fmp4_loudness* loudness) {
    return loudness->type;
}

FMP4_API
uint8_t
fmp4_loudness_get_downmix_id(const fmp4_loudness* loudness) {
    return loudness->downmix_id;
}

FMP4_API
uint8_t
fmp4_loudness_get_drc_id(const fmp4_loudness* loudness) {
    return loudness->drc_id;
}

FMP4_API
double
fmp4_loudness_get_bs_sample_peak_level(const fmp4_loudness* loudness) {
    if(loudness->sample_peak == 0) return INFINITY;
    return (((double)loudness->sample_peak) / -32.0f) + 20.0f;
}

FMP4_API
double
fmp4_loudness_get_bs_true_peak_level(const fmp4_loudness* loudness) {
    if(loudness->true_peak == 0) return INFINITY;
    return (((double)loudness->true_peak) / -32.0f) + 20.0f;
}

FMP4_API
uint8_t
fmp4_loudness_get_system(const fmp4_loudness* loudness) {
    return loudness->system;
}

FMP4_API
uint8_t
fmp4_loudness_get_reliability(const fmp4_loudness* loudness) {
    return loudness->reliability;
}

FMP4_API
size_t
fmp4_loudness_get_measurement_count(const fmp4_loudness* loudness) {
    return loudness->measurements.len / sizeof(fmp4_measurement*);
}

FMP4_API
fmp4_measurement*
fmp4_loudness_get_measurement(const fmp4_loudness* loudness, size_t index) {
    size_t count = loudness->measurements.len / sizeof(fmp4_measurement*);
    fmp4_measurement** measurements = (fmp4_measurement**)loudness->measurements.x;
    if(index < count) return measurements[index];
    return NULL;
}

FMP4_API
fmp4_measurement**
fmp4_loudness_get_measurements(const fmp4_loudness* loudness, size_t* count) {
    if(count != NULL) *count = loudness->measurements.len / sizeof(fmp4_measurement*);
    return (fmp4_measurement**)loudness->measurements.x;
}

FMP4_API
uint8_t
fmp4_measurement_get_method(const fmp4_measurement* measurement) {
    return measurement->method;
}

FMP4_PRIVATE
double
fmp4_decode_loudness_range(uint8_t val) {
    if(val <= 128) {
        return (((double)val) - 0.5f) / 4.0f;
    } else if(val <= 204) {
        return ((((double)val) - 128.5f) / 2.0f) + 32.0f;
    }
    return (((double)val) - 134.5f);
}

FMP4_API
double
fmp4_measurement_get_value(const fmp4_measurement* measurement) {
    switch(measurement->method) {
        case 1: /* fall-through */
        case 2: /* fall-through */
        case 3: /* fall-through */
        case 4: /* fall-through */
        case 5: return (((double)measurement->value) / 4.0f) - 57.75f;
        case 6: return fmp4_decode_loudness_range(measurement->value);
        case 7: return ((double)measurement->value) - 80.0f;
        case 8: return ((double)measurement->value);
        default: break;
    }
    return INFINITY;
}

FMP4_API
uint8_t
fmp4_measurement_get_system(const fmp4_measurement* measurement) {
    return measurement->system;
}

FMP4_API
uint8_t
fmp4_measurement_get_reliability(const fmp4_measurement* measurement) {
    return measurement->reliability;
}

FMP4_API
uint8_t
fmp4_sample_flags_get_is_leading(const fmp4_sample_flags* flags) {
    return flags->is_leading;
}

FMP4_API
uint8_t
fmp4_sample_flags_get_depends_on(const fmp4_sample_flags* flags) {
    return flags->depends_on;
}

FMP4_API
uint8_t
fmp4_sample_flags_get_is_depended_on(const fmp4_sample_flags* flags) {
    return flags->is_depended_on;
}

FMP4_API
uint8_t
fmp4_sample_flags_get_has_redundancy(const fmp4_sample_flags* flags) {
    return flags->has_redundancy;
}

FMP4_API
uint8_t
fmp4_sample_flags_get_padding_value(const fmp4_sample_flags* flags) {
    return flags->padding_value;
}

FMP4_API
uint8_t
fmp4_sample_flags_get_is_non_sync(const fmp4_sample_flags* flags) {
    return flags->is_non_sync;
}

FMP4_API
uint16_t
fmp4_sample_flags_get_degradation_priority(const fmp4_sample_flags* flags) {
    return flags->degradation_priority;
}

FMP4_API
uint32_t
fmp4_sample_info_get_duration(const fmp4_sample_info* info) {
    return info->duration;
}

FMP4_API
uint32_t
fmp4_sample_info_get_size(const fmp4_sample_info* info) {
    return info->size;
}

FMP4_API
uint32_t
fmp4_sample_info_get_sample_group(const fmp4_sample_info* info) {
    return info->sample_group;
}

FMP4_API
fmp4_result
fmp4_sample_info_get_flags(const fmp4_sample_info* info, fmp4_sample_flags* flags) {
    memcpy(flags,&info->flags,sizeof(fmp4_sample_flags));
    return FMP4_OK;
}

FMP4_API
uint8_t
fmp4_emsg_get_version(const fmp4_emsg* emsg) {
    return emsg->version;
}

FMP4_API
uint32_t
fmp4_emsg_get_timescale(const fmp4_emsg* emsg) {
    return emsg->timescale;
}

FMP4_API
uint32_t
fmp4_emsg_get_presentation_time_delta(const fmp4_emsg* emsg) {
    return emsg->presentation_time_delta;
}

FMP4_API
uint64_t
fmp4_emsg_get_presentation_time(const fmp4_emsg* emsg) {
    return emsg->presentation_time;
}

FMP4_API
uint32_t
fmp4_emsg_get_event_duration(const fmp4_emsg* emsg) {
    return emsg->event_duration;
}

FMP4_API
uint32_t
fmp4_emsg_get_id(const fmp4_emsg* emsg) {
    return emsg->id;
}

FMP4_API
const char*
fmp4_emsg_get_scheme_id_uri(const fmp4_emsg* emsg) {
    return (const char *)emsg->scheme_id_uri.x;
}

FMP4_API
const char*
fmp4_emsg_get_value(const fmp4_emsg* emsg) {
    return (const char *)emsg->value.x;
}

FMP4_API
const uint8_t*
fmp4_emsg_get_message(const fmp4_emsg* emsg, uint32_t* message_size) {
    if(message_size != NULL) *message_size = emsg->message.len;
    return emsg->message.x;
}



FMP4_API
void
fmp4_track_set_stream_type(fmp4_track* track, fmp4_stream_type stream_type) {
    track->stream_type = stream_type;
    return;
}

FMP4_API
void
fmp4_track_set_codec(fmp4_track* track, fmp4_codec codec) {
    track->codec = codec;
    return;
}

FMP4_API
void
fmp4_track_set_object_type(fmp4_track* track, fmp4_object_type object_type) {
    track->object_type = object_type;
    return;
}

FMP4_API
void
fmp4_track_set_base_media_decode_time(fmp4_track* track, uint64_t base_media_decode_time) {
    track->base_media_decode_time = base_media_decode_time;
    return;
}

FMP4_API
void
fmp4_track_set_time_scale(fmp4_track* track, uint32_t time_scale) {
    track->time_scale = time_scale;
    return;
}

FMP4_API
void
fmp4_track_set_language(fmp4_track* track, const char* language) {
    size_t l = strlen(language);
    if(l > 3) l = l;
    memcpy(track->language, language, 3);
    track->language[3] = '\0';
    return;
}

FMP4_API
void
fmp4_track_set_encoder_delay(fmp4_track* track, uint32_t delay) {
    track->encoder_delay = delay;
}

FMP4_API
void
fmp4_track_set_roll_distance(fmp4_track* track, int16_t distance) {
    track->roll_distance = distance;
}

FMP4_API
void
fmp4_track_set_roll_type(fmp4_track* track, fmp4_roll_type roll_type) {
    track->roll_type = roll_type;
}

FMP4_API
void
fmp4_track_set_audio_channels(fmp4_track* track, uint16_t channels) {
    track->info.audio.channels = channels;
    return;
}


FMP4_API
void
fmp4_track_set_default_sample_info(fmp4_track *track, const fmp4_sample_info* info) {
    memcpy(&track->default_sample_info,info,sizeof(fmp4_sample_info));
    return;
}

FMP4_API
fmp4_result
fmp4_track_set_dsi(fmp4_track* track, const void* dsi, size_t len) {
    track->dsi.len = 0;
    return fmp4_membuf_cat(&track->dsi,dsi,len);
}

FMP4_API
void
fmp4_track_set_first_sample_flags(fmp4_track* track, const fmp4_sample_flags* flags) {
    track->first_sample_flags = fmp4_encode_sample_flags(flags);
    return;
}

FMP4_API
void
fmp4_track_set_trun_sample_flags(fmp4_track* track, const fmp4_sample_flags* flags) {
    track->trun_sample_flags_set = 1;
    track->trun_sample_flags = fmp4_encode_sample_flags(flags);
    return;
}

FMP4_API
void
fmp4_track_set_trun_sample_duration(fmp4_track* track, uint32_t duration) {
    track->trun_sample_duration_set = 1;
    track->trun_sample_duration = duration;
    return;
}

FMP4_API
void
fmp4_track_set_trun_sample_size(fmp4_track* track, uint32_t size) {
    track->trun_sample_size_set = 1;
    track->trun_sample_size = size;
    return;
}

FMP4_API
void
fmp4_track_set_trun_sample_count(fmp4_track* track, uint64_t count) {
    track->trun_sample_count = count;
    return;
}

FMP4_API
void
fmp4_loudness_set_downmix_id(fmp4_loudness* loudness, uint8_t id) {
    loudness->downmix_id = id;
    return;
}

FMP4_API
void
fmp4_loudness_set_drc_id(fmp4_loudness* loudness, uint8_t id) {
    loudness->drc_id = id;
    return;
}

FMP4_API
void
fmp4_loudness_set_type(fmp4_loudness* loudness, fmp4_loudness_type type) {
    loudness->type = type;
    return;
}

FMP4_API
fmp4_result
fmp4_loudness_set_sample_peak(fmp4_loudness* loudness, double peak) {
    int64_t tmp;
    if(isinf(peak) || isnan(peak)) {
        loudness->sample_peak = 0;
    }
    else {
        tmp = (int64_t)((peak - 20.0f) * -32.0f);
        if(tmp < 0 || tmp > 0xFFFF) return FMP4_PEAKINVALID;
        loudness->sample_peak = (uint16_t)tmp;
    }
    return FMP4_OK;
}

FMP4_API
fmp4_result
fmp4_loudness_set_true_peak(fmp4_loudness* loudness, double peak) {
    int64_t tmp;
    if(isinf(peak) || isnan(peak)) {
        loudness->true_peak = 0;
    }
    else {
        tmp = (int64_t)((peak - 20.0f) * -32.0f);
        if(tmp < 0 || tmp > 0xFFFF) return FMP4_PEAKINVALID;
        loudness->true_peak = (uint16_t)tmp;
    }
    return FMP4_OK;
}

FMP4_API
fmp4_result
fmp4_loudness_set_system(fmp4_loudness* loudness, uint8_t system) {
    if(system > 5) return FMP4_SYSTEMINVALID;
    loudness->system = system;
    return FMP4_OK;
}

FMP4_API
fmp4_result
fmp4_loudness_set_reliability(fmp4_loudness* loudness, uint8_t reliability) {
    if(reliability > 3) return FMP4_RELIABILITYINVALID;
    loudness->reliability = reliability;
    return FMP4_OK;
}

FMP4_API
fmp4_result
fmp4_measurement_set_method(fmp4_measurement* measurement, uint8_t method) {
    if(method > 8) return FMP4_METHODINVALID;
    measurement->method = method;
    return FMP4_OK;
}

FMP4_PRIVATE
uint8_t
fmp4_encode_loudness_range(double val) {
    if(val < 0.0f) return 0;
    if(val <= 32.0f) return (uint8_t)(4.0f * val + 0.5f);
    if(val <= 70.0f) return ((uint8_t)(2.0f * (val - 32) + 0.5f)) + 128;
    if(val <= 121.0f) return (uint8_t)((val - 70.0f) + 0.5f) + 204;
    return 255;
}

FMP4_API
fmp4_result
fmp4_measurement_set_value(fmp4_measurement* measurement, double value) {
    switch(measurement->method) {
        case 1: /* fall-through */
        case 2: /* fall-through */
        case 3: /* fall-through */
        case 4: /* fall-through */
        case 5: measurement->value = (uint8_t)((value + 57.75) * 4.0f); break;
        case 6: measurement->value = fmp4_encode_loudness_range(value); break;
        case 7: measurement->value = (uint8_t)(value - 80.0f); break;
        case 8: measurement->value = (uint8_t)value; break;
        default: return FMP4_METHODINVALID;
    }
    return FMP4_OK;
}

FMP4_API
fmp4_result
fmp4_measurement_set_system(fmp4_measurement* measurement, uint8_t system) {
    if(system > 5) return FMP4_SYSTEMINVALID;
    measurement->system = system;
    return FMP4_OK;
}

FMP4_API
fmp4_result
fmp4_measurement_set_reliability(fmp4_measurement* measurement, uint8_t reliability) {
    if(reliability > 3) return FMP4_RELIABILITYINVALID;
    measurement->reliability = reliability;
    return FMP4_OK;
}

FMP4_API
void
fmp4_sample_flags_set_is_leading(fmp4_sample_flags* flags, uint8_t is_leading) {
    flags->is_leading = is_leading;
    return;
}

FMP4_API
void
fmp4_sample_flags_set_depends_on(fmp4_sample_flags* flags, uint8_t depends_on) {
    flags->depends_on = depends_on;
    return;
}

FMP4_API
void
fmp4_sample_flags_set_is_depended_on(fmp4_sample_flags* flags, uint8_t is_depended_on) {
    flags->is_depended_on = is_depended_on;
    return;
}

FMP4_API
void
fmp4_sample_flags_set_has_redundancy(fmp4_sample_flags* flags, uint8_t has_redundancy) {
    flags->has_redundancy = has_redundancy;
    return;
}

FMP4_API
void
fmp4_sample_flags_set_padding_value(fmp4_sample_flags* flags, uint8_t padding_value) {
    flags->padding_value = padding_value;
    return;
}

FMP4_API
void
fmp4_sample_flags_set_is_non_sync(fmp4_sample_flags* flags, uint8_t is_non_sync) {
    flags->is_non_sync = is_non_sync;
    return;
}

FMP4_API
void
fmp4_sample_flags_set_degradation_priority(fmp4_sample_flags* flags, uint16_t priority) {
    flags->degradation_priority = priority;
    return;
}

FMP4_API
void
fmp4_sample_info_set_duration(fmp4_sample_info* info, uint32_t duration) {
    info->duration = duration;
    return;
}

FMP4_API
void
fmp4_sample_info_set_size(fmp4_sample_info* info, uint32_t size) {
    info->size = size;
    return;
}

FMP4_API
void
fmp4_sample_info_set_sample_group(fmp4_sample_info* info, uint32_t sample_group) {
    info->sample_group = sample_group;
    return;
}

FMP4_API
void
fmp4_sample_info_set_flags(fmp4_sample_info* info, const fmp4_sample_flags* flags) {
    memcpy(&info->flags,flags,sizeof(fmp4_sample_flags));
    return;
}

FMP4_API
fmp4_result
fmp4_emsg_set_version(fmp4_emsg* emsg, uint8_t version) {
    if(version > 1) return FMP4_INVALIDEMSGVER;
    emsg->version = version;
    return FMP4_OK;
}

FMP4_API
void
fmp4_emsg_set_timescale(fmp4_emsg* emsg, uint32_t timescale) {
    emsg->timescale = timescale;
    return;
}

FMP4_API
void
fmp4_emsg_set_presentation_time_delta(fmp4_emsg* emsg, uint32_t presentation_time_delta) {
    emsg->presentation_time_delta = presentation_time_delta;
    return;
}

FMP4_API
void
fmp4_emsg_set_presentation_time(fmp4_emsg* emsg, uint64_t presentation_time) {
    emsg->presentation_time = presentation_time;
    return;
}

FMP4_API
void
fmp4_emsg_set_event_duration(fmp4_emsg* emsg, uint32_t event_duration) {
    emsg->event_duration = event_duration;
    return;
}

FMP4_API
void
fmp4_emsg_set_id(fmp4_emsg* emsg, uint32_t id) {
    emsg->id = id;
    return;
}

FMP4_API
fmp4_result
fmp4_emsg_set_scheme_id_uri(fmp4_emsg* emsg, const char* scheme_id_uri) {
    emsg->scheme_id_uri.len = 0;
    return fmp4_membuf_cat(&emsg->scheme_id_uri, scheme_id_uri, strlen(scheme_id_uri) + 1);
}

FMP4_API
fmp4_result
fmp4_emsg_set_value(fmp4_emsg* emsg, const char* value) {
    emsg->value.len = 0;
    return fmp4_membuf_cat(&emsg->value, value, strlen(value) + 1);
}

FMP4_API
fmp4_result
fmp4_emsg_set_message(fmp4_emsg* emsg, const uint8_t* message_data, uint32_t message_size) {
    emsg->message.len = 0;
    return fmp4_membuf_cat(&emsg->message, message_data, message_size);
}


FMP4_API
size_t
fmp4_mux_size(void) {
    return sizeof(fmp4_mux);
}

FMP4_API
size_t
fmp4_track_size(void) {
    return sizeof(fmp4_track);
}

FMP4_API
size_t
fmp4_sample_info_size(void) {
    return sizeof(fmp4_sample_info);
}

FMP4_API
size_t
fmp4_loudness_size(void) {
    return sizeof(fmp4_loudness);
}

FMP4_API
size_t
fmp4_measurement_size(void) {
    return sizeof(fmp4_measurement);
}

FMP4_API
size_t
fmp4_emsg_size(void) {
    return sizeof(fmp4_emsg);
}


FMP4_API
void
fmp4_mux_init(fmp4_mux* mux, const fmp4_allocator* allocator) {
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    mux->allocator = allocator;

    fmp4_membuf_init(&mux->buffer, mux->allocator);
    fmp4_membuf_init(&mux->stack,  mux->allocator);
    fmp4_membuf_init(&mux->brands, mux->allocator);
    fmp4_membuf_init(&mux->tracks, mux->allocator);
    fmp4_membuf_init(&mux->emsgs,  mux->allocator);
    fmp4_membuf_init(&mux->alloc_track, mux->allocator);
    fmp4_membuf_init(&mux->alloc_emsg,  mux->allocator);
    mux->brand_minor_version = 0;
    mux->fragments = 0;
    mux->moof_offset = 0;

    /* set a default major brand */
    fmp4_mux_set_brand_major(mux, "iso6", 0);
    return;
}

FMP4_API
void
fmp4_sample_flags_init(fmp4_sample_flags* flags) {
    flags->is_leading = 0;
    flags->depends_on = 0;
    flags->is_depended_on = 0;
    flags->has_redundancy = 0;
    flags->padding_value = 0;
    flags->is_non_sync = 0;
    flags->degradation_priority = 0;
    return;
}


FMP4_API
void
fmp4_sample_info_init(fmp4_sample_info* sample_info) {
    sample_info->duration = 0;
    sample_info->size = 0;
    sample_info->sample_group = 0;
    fmp4_sample_flags_init(&sample_info->flags);
    return;
}


FMP4_API
void
fmp4_track_init(fmp4_track *track, const fmp4_allocator* allocator) {
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    track->allocator = allocator;

    track->stream_type = FMP4_STREAM_TYPE_FORBIDDEN;
    track->base_media_decode_time = 0;
    track->time_scale = 0;
    track->encoder_delay = 0;
    track->roll_distance = 0;
    track->roll_type = FMP4_ROLL_TYPE_ROLL;

    fmp4_sample_info_init(&track->default_sample_info);

    track->trun_sample_flags_set     = 0;
    track->trun_sample_flags         = 0;
    track->trun_sample_duration_set  = 0;
    track->trun_sample_duration      = 0;
    track->trun_sample_size_set      = 0;
    track->trun_sample_size          = 0;

    track->trun_sample_count       = 0;
    track->data_offset_pos         = 0;

    memset(track->language,0,sizeof(track->language));

    track->codec = FMP4_CODEC_UNDEFINED;
    track->object_type = FMP4_OBJECT_TYPE_FORBIDDEN;
    track->info.audio.channels = 0;

    fmp4_membuf_init(&track->sample_info, track->allocator);
    fmp4_membuf_init(&track->mdat,        track->allocator);
    fmp4_membuf_init(&track->dsi,         track->allocator);
    fmp4_membuf_init(&track->loudness,    track->allocator);
    fmp4_membuf_init(&track->alloc_loudness, track->allocator);
    return;
}

FMP4_API
void
fmp4_loudness_init(fmp4_loudness* loudness, const fmp4_allocator* allocator) {
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    loudness->allocator = allocator;

    loudness->type = FMP4_LOUDNESS_UNDEF;
    loudness->downmix_id  = 0;
    loudness->drc_id      = 0;
    loudness->sample_peak = 0;
    loudness->true_peak   = 0;
    loudness->system      = 0;
    loudness->reliability = 0;

    fmp4_membuf_init(&loudness->measurements, loudness->allocator);
    fmp4_membuf_init(&loudness->alloc_measurement, loudness->allocator);
    return;
}

FMP4_API
void
fmp4_measurement_init(fmp4_measurement* measurement) {
    measurement->method = 0;
    measurement->value = 0;
    measurement->system = 0;
    measurement->reliability = 0;
    return;
}

FMP4_API
void
fmp4_emsg_init(fmp4_emsg* emsg, const fmp4_allocator* allocator) {
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    emsg->allocator = allocator;

    emsg->version = 0;
    emsg->timescale = 0;
    emsg->presentation_time_delta = 0;
    emsg->presentation_time = 0;
    emsg->event_duration = 0;
    emsg->id = 0;
    fmp4_membuf_init(&emsg->scheme_id_uri, emsg->allocator);
    fmp4_membuf_init(&emsg->value,         emsg->allocator);
    fmp4_membuf_init(&emsg->message,       emsg->allocator);
    return;
}

FMP4_API
fmp4_mux*
fmp4_mux_new(const fmp4_allocator* allocator) {
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    fmp4_mux* mux = (fmp4_mux*)allocator->malloc(sizeof(fmp4_mux),allocator->userdata);
    if(mux != NULL) fmp4_mux_init(mux, allocator);
    return mux;
}

FMP4_API
fmp4_track*
fmp4_track_new(const fmp4_allocator* allocator) {
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    fmp4_track* track = (fmp4_track*)allocator->malloc(sizeof(fmp4_track),allocator->userdata);
    if(track != NULL) fmp4_track_init(track,allocator);
    return track;
}

FMP4_API
fmp4_loudness*
fmp4_loudness_new(const fmp4_allocator* allocator) {
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    fmp4_loudness* loudness = (fmp4_loudness*)allocator->malloc(sizeof(fmp4_loudness),allocator->userdata);
    if(loudness != NULL) fmp4_loudness_init(loudness,allocator);
    return loudness;
}

FMP4_API
fmp4_sample_info*
fmp4_sample_info_new(const fmp4_allocator* allocator) {
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    fmp4_sample_info* sample_info = (fmp4_sample_info*)allocator->malloc(sizeof(fmp4_sample_info),allocator->userdata);
    if(sample_info != NULL) {
        fmp4_sample_info_init(sample_info);
        sample_info->allocator = allocator;
    }
    return sample_info;
}

FMP4_API
fmp4_sample_flags*
fmp4_sample_flags_new(const fmp4_allocator* allocator) {
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    fmp4_sample_flags* sample_flags = (fmp4_sample_flags*)allocator->malloc(sizeof(fmp4_sample_flags),allocator->userdata);
    if(sample_flags != NULL) {
        fmp4_sample_flags_init(sample_flags);
        sample_flags->allocator = allocator;
    }
    return sample_flags;
}

FMP4_API
fmp4_measurement*
fmp4_measurement_new(const fmp4_allocator* allocator) {
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    fmp4_measurement* measurement = (fmp4_measurement*)allocator->malloc(sizeof(fmp4_measurement),allocator->userdata);
    if(measurement != NULL) {
        fmp4_measurement_init(measurement);
        measurement->allocator = allocator;
    }
    return measurement;
}

FMP4_API
fmp4_emsg*
fmp4_emsg_new(const fmp4_allocator* allocator) {
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    fmp4_emsg* emsg = (fmp4_emsg*)allocator->malloc(sizeof(fmp4_emsg),allocator->userdata);
    if(emsg != NULL) fmp4_emsg_init(emsg,allocator);
    return emsg;
}


FMP4_API
void
fmp4_mux_close(fmp4_mux* mux) {
    size_t i;
    size_t len;

    fmp4_track** tracks;
    fmp4_emsg** emsgs;

    fmp4_membuf_free(&mux->tracks);
    fmp4_membuf_free(&mux->buffer);
    fmp4_membuf_free(&mux->stack);
    fmp4_membuf_free(&mux->brands);
    fmp4_membuf_free(&mux->emsgs);

    tracks = (fmp4_track**)mux->alloc_track.x;
    len = mux->alloc_track.len / sizeof(fmp4_track*);
    for(i=0;i<len;i++) {
        fmp4_track_free(tracks[i]);
    }

    emsgs = (fmp4_emsg**)mux->alloc_emsg.x;
    len = mux->alloc_emsg.len / sizeof(fmp4_emsg*);
    for(i=0;i<len;i++) {
        fmp4_emsg_free(emsgs[i]);
    }

    fmp4_membuf_free(&mux->alloc_track);
    fmp4_membuf_free(&mux->alloc_emsg);

    return;
}

FMP4_API
void
fmp4_track_close(fmp4_track *track) {
    size_t i;
    size_t len;

    fmp4_loudness** loudnesses;

    fmp4_membuf_free(&track->sample_info);
    fmp4_membuf_free(&track->mdat);
    fmp4_membuf_free(&track->loudness);
    fmp4_membuf_free(&track->dsi);

    loudnesses = (fmp4_loudness**)track->alloc_loudness.x;
    len = track->alloc_loudness.len / sizeof(fmp4_loudness*);
    for(i=0;i<len;i++) {
        fmp4_loudness_free(loudnesses[i]);
    }
    fmp4_membuf_free(&track->alloc_loudness);

    return;
}

FMP4_API
void
fmp4_loudness_close(fmp4_loudness* loudness) {
    size_t i;
    size_t len;

    fmp4_measurement** measurements;

    fmp4_membuf_free(&loudness->measurements);

    measurements = (fmp4_measurement**)loudness->alloc_measurement.x;
    len = loudness->alloc_measurement.len / sizeof(fmp4_measurement*);
    for(i=0;i<len;i++) {
        fmp4_measurement_free(measurements[i]);
    }
    fmp4_membuf_free(&loudness->alloc_measurement);
    return;
}

FMP4_API
void
fmp4_emsg_close(fmp4_emsg* emsg) {
    fmp4_membuf_free(&emsg->scheme_id_uri);
    fmp4_membuf_free(&emsg->value);
    fmp4_membuf_free(&emsg->message);
    return;
}

FMP4_API
void
fmp4_mux_free(fmp4_mux* mux) {
    const fmp4_allocator *allocator = mux->allocator;
    if(allocator == NULL) allocator = &fmp4_default_allocator;

    fmp4_mux_close(mux);
    allocator->free(mux,allocator->userdata);
    return;
}

FMP4_API
void
fmp4_track_free(fmp4_track *track) {
    const fmp4_allocator *allocator = track->allocator;
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    fmp4_track_close(track);
    allocator->free(track,allocator->userdata);
    return;
}

FMP4_API
void
fmp4_sample_info_free(fmp4_sample_info* sample_info) {
    const fmp4_allocator *allocator = sample_info->allocator;
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    allocator->free(sample_info,allocator->userdata);
    return;
}

FMP4_API
void
fmp4_sample_flags_free(fmp4_sample_flags* sample_flags) {
    const fmp4_allocator *allocator = sample_flags->allocator;
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    allocator->free(sample_flags,allocator->userdata);
    return;
}

FMP4_API
void
fmp4_loudness_free(fmp4_loudness* loudness) {
    const fmp4_allocator *allocator = loudness->allocator;
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    fmp4_loudness_close(loudness);
    allocator->free(loudness,allocator->userdata);
    return;
}

FMP4_API
void
fmp4_measurement_free(fmp4_measurement* measurement) {
    const fmp4_allocator *allocator = measurement->allocator;
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    allocator->free(measurement,allocator->userdata);
    return;
}

FMP4_API
void
fmp4_emsg_free(fmp4_emsg* emsg) {
    const fmp4_allocator *allocator = emsg->allocator;
    if(allocator == NULL) allocator = &fmp4_default_allocator;
    fmp4_emsg_close(emsg);
    allocator->free(emsg,allocator->userdata);
    return;
}

#endif

/*

Copyright (c) 2022 John Regan

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
