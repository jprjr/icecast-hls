#include "../src/version.h"
#include <stdio.h>

int main(void) {
    printf("%d.%d.%d\n",
      ICECAST_HLS_VERSION_MAJOR,
      ICECAST_HLS_VERSION_MINOR,
      ICECAST_HLS_VERSION_PATCH);
    return 0;
}
