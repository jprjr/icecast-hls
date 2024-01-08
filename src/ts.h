#ifndef ICH_TS_H
#define ICH_TS_H

#define TS_PACKET_SIZE 188
#define TS_HEADER_SIZE 4
#define TS_MAX_PAYLOAD_SIZE 184

#define TS_MAX_PACKET_SIZE 65527

#include "membuf.h"
#include "codecs.h"

struct mpegts_header {
    uint8_t tei;
    uint8_t pusi;
    uint8_t prio;
    uint16_t pid;
    uint8_t tsc;
    uint8_t adapt;
    uint8_t cc;
};

typedef struct mpegts_header mpegts_header;

struct mpegts_adaptation_field {
    uint8_t discontinuity;
    uint8_t random_access_error;
    uint8_t es_priority;
    uint8_t pcr_flag;
    uint8_t opcr_flag;
    uint8_t splicing_point_flag;
    int8_t splice_countdown;
    const membuf* transport_private_data;
    /* TODO (maybe): adaptation extension */
    uint8_t stuffing;

    uint64_t pcr_base;
    uint16_t pcr_extension;
    uint64_t opcr_base;
    uint16_t opcr_extension;
};

typedef struct mpegts_adaptation_field mpegts_adaptation_field;

struct mpegts_pes_header {
    uint8_t stream_id;
    uint16_t packet_length;
    uint64_t pts;
    uint8_t stuffing;
};
typedef struct mpegts_pes_header mpegts_pes_header;

struct mpegts_stream {
    uint8_t stream_id;
    uint64_t pts;
    mpegts_header header;
    mpegts_adaptation_field adaptation;
};

typedef struct mpegts_stream mpegts_stream;


#ifdef __cplusplus
extern "C" {
#endif

void mpegts_header_init(mpegts_header*);
void mpegts_adaptation_field_init(mpegts_adaptation_field*);

void mpegts_stream_init(mpegts_stream*);

/* returns how many bytes the adaptation field will
 * require to encode, minus the field length byte */
uint8_t mpegts_adaptation_field_length(const mpegts_adaptation_field *);

int mpegts_header_encode(membuf* dest, const mpegts_header *tsh);
int mpegts_adaptation_field_encode(membuf* dest, const mpegts_adaptation_field *);

int mpegts_pes_packet_header_encode(membuf* dest, const mpegts_pes_header *header);

int mpegts_stream_encode_packet(membuf* dest, mpegts_stream *stream, const membuf* data);

/* encodes a PAT, should be called directly after mpegts_header_encode, assumes only 1 program, 1 PAT, etc */
int mpegts_pat_encode(membuf* dest, uint16_t program_map_pid);

/* encodes a PMT, should be called directly after mpegts_header_encode, assumes 1 stream type + timed_id3,
 * setting id3_pid to 0 indicates no ID3 data */
int mpegts_pmt_encode(membuf* dest, codec_type codec, uint16_t audio_pid, uint16_t id3_pid);

int mpegts_packet_reset(membuf* packet, uint8_t fill);

#ifdef __cplusplus
}
#endif

#endif
