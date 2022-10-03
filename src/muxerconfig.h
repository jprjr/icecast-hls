#ifndef MUXERCONFIG_H
#define MUXERCONFIG_H

#include "codecs.h"
#include "membuf.h"
#include "muxerinfo.h"


/* a struct used for encoders to inform muxers of
 * needed muxer config - like AAC Audio Config
 * for mp4.
 *
 * This allows muxers to reject muxer types it can't handle,
 * for example, a packed muxer muxer can only handle
 * MP3 and AAC, and can reject any other muxer types.
 * */

struct muxerconfig {
    muxerinfo_handler info; /* used to send info back to the encoder */
    codec_type type;
    unsigned int channels;
    unsigned int sample_rate;
    unsigned int frame_len;
    unsigned int sync_flag; /* if non-zero, all samples are sync samples */
};

typedef struct muxerconfig muxerconfig;

#define MUXERCONFIG_ZERO { .info = MUXERINFO_HANDLER_ZERO, .type = CODEC_TYPE_UNKNOWN, .channels = 0, .sample_rate = 0, .frame_len = 0, .sync_flag = 0 }

typedef int (*muxerconfig_handler_callback)(void* userdata, const muxerconfig* config);

/* used to submit dsi to the muxer */
typedef int (*muxerconfig_dsi_callback)(void* userdata, const membuf* dsi);

struct muxerconfig_handler {
    muxerconfig_handler_callback submit;
    muxerconfig_dsi_callback submit_dsi;
    void* userdata;
};

typedef struct muxerconfig_handler muxerconfig_handler;


#define MUXERCONFIG_HANDLER_ZERO { .submit = muxerconfig_null_submit, .submit_dsi = muxerconfig_null_dsi, .userdata = NULL }

#ifdef __cplusplus
extern "C" {
#endif

extern const muxerconfig_handler muxerconfig_ignore;
int muxerconfig_null_submit(void *, const muxerconfig*);
int muxerconfig_null_dsi(void *, const membuf*);

#ifdef __cplusplus
}
#endif

#endif

