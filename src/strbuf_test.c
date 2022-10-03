#include "strbuf.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    strbuf s1;
    strbuf s2;
    char* t;

    membuf_init(&s1);
    membuf_init(&s2);

    membuf_append(&s1,"Hello.txt",9);
    membuf_append(&s2,"Hello.PDF",9);

    printf("strbuf_casecmp: %d\n",strbuf_casecmp(&s1,&s2));

    membuf_free(&s1);
    membuf_free(&s2);

    return 0;
}
