#ifndef HLS_H
#define HLS_H

/* A struct used by the folder and http outputs to
 * track the current state of HLS and generate playlists.
 *
 * The folder/http type plugins submit chunks, which may
 * not be full, complete segments (ie, the hls output
 * may be generating 6-second files but the fmp4 muxer
 * is generating 1-second chunks). So the hls object
 * will buffer these, then use a callback to write out
 * a file. It then stores the metadata (filename, HLS playlist
 * tags) about the segment into a circular buffer. When
 * the buffer length is reached, old metadata is cleared.
 *
 * When old metadata is cleared, a callback is fired
 * to (maybe) delete the segment. This is user-configurable,
 * since if you're using say, an HTTP output and relaying
 * chunks to AWS, it's (maybe?) more cost-effective to just let
 * old segments expire.
 */

#include "strbuf.h"
#include "segment.h"
#include "picture.h"
#include "ich_time.h"

typedef int(*hls_write_callback)(void* userdata, const strbuf* filename, const membuf* data, const strbuf* mime);
typedef int(*hls_delete_callback)(void* userdata, const strbuf* filename);

/* the metadata that gets stored per full file segment */
struct hls_segment_meta {
    strbuf filename;
    strbuf tags; /* stores everything like #EXTINF, #PROGRAMDATETIME and
                 the actual filename portion, which may be prefixed
                 with a URL or something */
    strbuf expired_files; /* stores a list of strings that are considered expired when this chunk expires,
                             this is a list of NULL-separated c strings */
    uint8_t disc; /* a flag that, if sets, means this segment is discontinuous */
};

typedef struct hls_segment_meta hls_segment_meta;

/* buffers chunks of data from the muxer */
struct hls_segment {
    membuf data;
    unsigned int samples;
    uint64_t pts;
    strbuf expired_files;
};

typedef struct hls_segment hls_segment;

/* represents the current playlist in a circular buffer */
struct hls_playlist {
    hls_segment_meta* segments;
    size_t size; /* stores the actual size, which is capacity +1 */
    size_t head;
    size_t tail;
};

typedef struct hls_playlist hls_playlist;

struct hls_callback_handler {
    hls_write_callback write;
    hls_delete_callback delete;
    void* userdata;
};

typedef struct hls_callback_handler hls_callback_handler;

struct hls {
    strbuf txt; /* stores the actual, generated playlist */
    strbuf header; /* stores the header-type info */
    strbuf playlist_filename;
    strbuf playlist_mimetype;
    strbuf init_filename;
    strbuf init_mimetype;  /* the mimetype to use on init segments */
    strbuf segment_format; /* generated snprintf-style format string for media segments */
    strbuf segment_mimetype;  /* the mimetype to use on media segments */
    strbuf entry_prefix; /* user-configurable prefix on playlist entries */

    hls_playlist playlist;
    hls_segment segment;
    hls_callback_handler callbacks;
    unsigned int time_base;
    unsigned int target_duration; /* in seconds */
    unsigned int playlist_length; /* in seconds */
    unsigned int target_samples; /* target duration in samples */
    size_t media_sequence;        /* current media sequence number */
    size_t disc_sequence; /* current discontinuity sequence number */
    size_t counter;
    unsigned int version;         /* reported HLS playlist version */
    ich_time now;
    uint8_t has_init;
    uint64_t last_pts;
    uint64_t next_pts;
};

typedef struct hls hls;

#ifdef __cplusplus
extern "C" {
#endif

void hls_segment_init(hls_segment*);
void hls_segment_free(hls_segment*);

void hls_segment_reset(hls_segment*);

void hls_segment_meta_init(hls_segment_meta*);
void hls_segment_meta_free(hls_segment_meta*);

/* just resets the strbuffers, does not free */
void hls_segment_meta_reset(hls_segment_meta*);

void hls_playlist_init(hls_playlist*);
void hls_playlist_free(hls_playlist*);

int hls_playlist_open(hls_playlist*, unsigned int segments);
int hls_playlist_isempty(const hls_playlist*);
int hls_playlist_isfull(const hls_playlist*);
size_t hls_playlist_used(const hls_playlist*);
size_t hls_playlist_avail(const hls_playlist*);

/* returns a pointer to the next slot */
hls_segment_meta* hls_playlist_push(hls_playlist*);

/* removes a segment from the buffer */
hls_segment_meta* hls_playlist_shift(hls_playlist*);

/* gets a segment - does not remove */
hls_segment_meta* hls_playlist_get(hls_playlist*, size_t index);

void hls_init(hls*);
void hls_free(hls*);

int hls_configure(hls*, const strbuf* key, const strbuf* val);
int hls_open(hls*, const segment_source* source);

/* buffers a segment and maybe triggers writing callbacks */
int hls_add_segment(hls*, const segment* seg);

/* mark a file as expired whenever the current segment expires,
 * to issue a delete callback */
int hls_expire_file(hls*, const strbuf* filename);

/* writes out a partial segment if it exists */
int hls_flush(hls*);

/* flushes the last segment, adds #EXT-X-ENDLIST to the playlist, writes it */
int hls_close(hls*);

/* used by the outputs when we're trying to move a picture out-of-band,
 * write the picture out via callback, return picture URL in out */
int hls_submit_picture(hls*, const picture* src, picture* out);

const strbuf* hls_get_playlist(const hls* h);

#ifdef __cplusplus
}
#endif

#endif
