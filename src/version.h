#ifndef VERSIONH
#define VERSIONH

#define ICECAST_HLS_VERSION_MAJOR 1
#define ICECAST_HLS_VERSION_MINOR 1
#define ICECAST_HLS_VERSION_PATCH 0

#ifdef __cplusplus
extern "C" {
#endif

const char* icecast_hls_version_string(void);

#ifdef __cplusplus
}
#endif

#endif
