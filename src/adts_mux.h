#ifndef ICH_ADTS_MUX_H
#define ICH_ADTS_MUX_H

#include <stdint.h>
#include <stddef.h>

/* ADTS uses a 13-bit value for size,
 * which includes the header (7 bytes),
 * so 8191 - 7 */

#define ADTS_MAX_PACKET_SIZE 8184

/* we don't bother having fields for things like
 * the syncword (all 1s), layer (always 0), etc */

struct adts_mux {
    uint8_t buffer[8191];
    uint16_t len;

    uint8_t version;
    uint8_t profile;
    uint8_t sample_rate_index;
    uint8_t ch_index;
    uint8_t originality;
    uint8_t home;
    uint8_t copyright;
    uint8_t copyright_start;
};

typedef struct adts_mux adts_mux;

#define ADTS_MUX_ZERO { .len = 0, .version = 0, .profile = 0, .sample_rate_index = 0, .ch_index = 0, \
  .originality = 0, .home = 0, .copyright = 0, .copyright_start = 0 }

extern const adts_mux adts_mux_zero;

#ifdef __cplusplus
extern "C" {
#endif

void adts_mux_init(adts_mux *);

int adts_mux_encode_packet(adts_mux *mux, const void* data, size_t len);

int adts_mux_set_sample_rate(adts_mux* mux, unsigned int sample_rate);

int adts_mux_set_profile(adts_mux* mux, unsigned int profile);

int adts_mux_set_channel_layout(adts_mux* mux, uint64_t channel_layout);

#ifdef __cplusplus
}
#endif

#endif
