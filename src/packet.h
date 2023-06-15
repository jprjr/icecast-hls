#ifndef PACKET_H
#define PACKET_H

#include "membuf.h"
#include "strbuf.h"
#include "codecs.h"

struct packet {
    membuf data;
    unsigned int duration;
    unsigned int sample_rate;
    uint64_t pts;
    uint8_t sync; /* 1 if the packet is a sync packet */
};
typedef struct packet packet;

struct packet_source_info {
    unsigned int time_base;
    unsigned int frame_len;
};
typedef struct packet_source_info packet_source_info;

/* this struct is sent by muxers (packet receivers) to
 * set params on the encoder (packet source) */
struct packet_source_params {
    size_t segment_length; /* given in milliseconds */
    size_t packets_per_segment;
};
typedef struct packet_source_params packet_source_params;

/* state that the next (x) packets need to be keyframe packets,
 * used by the ogg muxer + libopus */
typedef int (*packet_source_set_keyframes_cb)(void* handle, unsigned int keyframes);

/* used to reset the state entirely - used by the ogg muxer + avcodec,
 * will flush the codec and send remaining packets to cb */
typedef int (*packet_source_reset_cb)(void* handle, void* dest, int (*cb)(void*, const packet*));

/* this struct is a small wrapper around the encoder (packet source),
 * used during the muxer open (packet receiver) so it knows
 * some stuff about the incoming packets - default frame length, sample rate,
 * the codec, etc */
struct packet_source {
    void* handle;
    const strbuf* name;
    packet_source_set_keyframes_cb set_keyframes;
    packet_source_reset_cb reset;
    codec_type codec;
    unsigned int profile; /* codec-specific profile */
    unsigned int channels;
    unsigned int sample_rate;
    unsigned int frame_len;
    unsigned int sync_flag; /* if non-zero, all samples are sync samples */
    unsigned int padding; /* number of samples that need to be discarded */
    int roll_distance; /* number of frames that need to be discarded, -1 means 1 frame before current */
    uint8_t roll_type; /* roll type, 0 = roll, 1 = prol */
    const membuf* dsi;
};

typedef struct packet_source packet_source;

/* a packet receiver represents a muxer - it receives packets from
 * the encoder. It has an open function (mapped to a muxer_open),
 * so the encoder can tell the a muxer about packet info */

struct packet_receiver_caps {
    uint8_t has_global_header; /* muxer supports global headers */
};
typedef struct packet_receiver_caps packet_receiver_caps;

typedef int (*packet_receiver_open_cb)(void* handle, const packet_source* source);
typedef int (*packet_receiver_submit_packet_cb)(void* handle, const packet*);
typedef int (*packet_receiver_flush_cb)(void* handle);
typedef uint32_t (*packet_receiver_get_caps_cb)(void* handle);
typedef int (*packet_receiver_get_segment_info_cb)(const void* userdata, const packet_source_info*, packet_source_params*);

struct packet_receiver {
    void* handle;
    packet_receiver_open_cb open;
    packet_receiver_submit_packet_cb submit_packet;
    packet_receiver_flush_cb flush;
    packet_receiver_get_caps_cb get_caps;
    packet_receiver_get_segment_info_cb get_segment_info;
};

typedef struct packet_receiver packet_receiver;

/* a few defines for allocating stuff on the stack */
#define PACKET_ZERO { \
    .data = MEMBUF_ZERO,\
    .duration = 0, \
    .sync = 0, \
    .sample_rate = 0, \
    .pts = 0 \
}

#define PACKET_SOURCE_INFO_ZERO { .time_base = 0, .frame_len = 0 }
#define PACKET_SOURCE_PARAMS_ZERO { .segment_length = 0, .packets_per_segment = 0 }

#define PACKET_RECEIVER_CAPS_ZERO { .has_global_header = 0 }

#define PACKET_RECEIVER_ZERO { \
    .handle = NULL, \
    .open = packet_receiver_open_null, \
    .submit_packet = packet_receiver_submit_packet_null,\
    .flush = packet_receiver_flush_null, \
    .get_caps = packet_receiver_get_caps_null, \
    .get_segment_info = packet_receiver_get_segment_info_null, \
}

#define PACKET_SOURCE_ZERO { \
    .handle = NULL, \
    .name = NULL, \
    .set_keyframes = packet_source_set_keyframes_null, \
    .reset = packet_source_reset_null, \
    .codec = CODEC_TYPE_UNKNOWN, \
    .channels = 0, \
    .sample_rate = 0, \
    .frame_len = 0, \
    .sync_flag = 0, \
    .padding = 0, \
    .roll_distance = 0, \
    .roll_type = 0, \
}

#ifdef __cplusplus
extern "C" {
#endif

void packet_init(packet*);
void packet_free(packet*);
int packet_copy(packet* dest, const packet* source);

int packet_set_data(packet*, const void* src, size_t len);

int packet_receiver_open_null(void* handle, const packet_source* source);
int packet_receiver_submit_packet_null(void* handle, const packet* packet);
int packet_receiver_flush_null(void* handle);
uint32_t packet_receiver_get_caps_null(void* handle);
int packet_receiver_get_segment_info_null(const void* userdata, const packet_source_info*, packet_source_params*);

int packet_source_set_keyframes_null(void* handle, unsigned int keyframes);
int packet_source_reset_null(void* handle, void* dest, int (*cb)(void*, const packet*));

extern const packet packet_zero;
extern const packet_receiver packet_receiver_zero;
extern const packet_source packet_source_zero;
extern const packet_source_params packet_source_params_zero;

#ifdef __cplusplus
}
#endif

#endif
