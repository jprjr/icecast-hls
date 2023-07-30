#include "version.h"

#define STR(x) #x
#define XSTR(x) STR(x)
#define VERSION_STRING "icecast-hls " XSTR(ICECAST_HLS_VERSION_MAJOR) "." XSTR(ICECAST_HLS_VERSION_MINOR) "." XSTR(ICECAST_HLS_VERSION_PATCH)


const char* icecast_hls_version_string(void) {
    return VERSION_STRING;
}
