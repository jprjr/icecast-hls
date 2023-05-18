#include "version.h"

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0
#define STR(x) #x
#define XSTR(x) STR(x)
#define VERSION_STRING "icecast-hls " XSTR(VERSION_MAJOR) "." XSTR(VERSION_MINOR) "." XSTR(VERSION_PATCH)


const char* icecast_hls_version_string(void) {
    return VERSION_STRING;
}
