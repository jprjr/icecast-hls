#ifndef MUXERINFO_H
#define MUXERINFO_H

/* used to get info from a muxer, back into the encoder via callback, like
 * if the muxer has a target segment duration, we can inform the encoder
 * of how many packets we expect per segment */
struct muxerinfo {
    unsigned int packets_per_segment;
};

typedef struct muxerinfo muxerinfo;

#define MUXERINFO_ZERO { .packets_per_segment = 0 }

typedef int (*muxerinfo_submit_callback)(void*, const muxerinfo*);
struct muxerinfo_handler {
    muxerinfo_submit_callback submit;
    void* userdata;
};

typedef struct muxerinfo_handler muxerinfo_handler;
#define MUXERINFO_HANDLER_ZERO { .submit = muxerinfo_null_submit, .userdata = NULL }

#ifdef __cplusplus
extern "C" {
#endif

extern const muxerinfo_handler muxerinfo_ignore;
int muxerinfo_null_submit(void*, const muxerinfo*);

#ifdef __cplusplus
}
#endif

#endif
