#include "str.h"
#include "strbuf.h"

#include <string.h>

int str_begins(const char *s, const char *q) {
    strbuf sb;
    strbuf qb;

    sb.x = (uint8_t *)s;
    sb.len = strlen(s);

    qb.x = (uint8_t *)q;
    qb.len = strlen(q);
    return strbuf_begins(&sb,&qb);
}

int str_end(const char *s, const char *q) {
    strbuf sb;
    strbuf qb;

    sb.x = (uint8_t *)s;
    sb.len = strlen(s);

    qb.x = (uint8_t *)q;
    qb.len = strlen(q);
    return strbuf_ends(&sb,&qb);
}
