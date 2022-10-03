#ifndef STRBUF_H
#define STRBUF_H

#include "membuf.h"

typedef membuf strbuf;

#define STRBUF_CONST(name,data) const strbuf name = { .a = 0, .len = sizeof(data)-1, .x = (uint8_t*)data }

#define STRBUF_ZERO { .a = 0, .len = 0, .x = NULL, .blocksize = 16 }

void strbuf_init(strbuf*);
void strbuf_init_bs(strbuf*,size_t);
void strbuf_free(strbuf*);
void strbuf_reset(strbuf*);

int strbuf_ready(strbuf*,size_t);
int strbuf_readyplus(strbuf*,size_t);

/* these allocate memory, you should call strbuf_free
 * when done */
int strbuf_copy(strbuf* d, const strbuf* src);
int strbuf_cat(strbuf* d, const strbuf* src);

int strbuf_append(strbuf* d, const char* src, size_t len);
int strbuf_append_cstr(strbuf* d, const char* src);

int strbuf_sprintf(strbuf* d, const char *fmt, ...);

/* add a terminator to a strbuf */
int strbuf_term(strbuf* d);

/* like strchr strrchr - returns a pointer inside of strbuf */
char* strbuf_chr(const strbuf* s, char c);
char* strbuf_rchr(const strbuf* s, char c);

/* ilke strchr and strrchr - but stores the pointer
 * and length into destination strbuf struct, does
 * NOT allocate memory / make a copy or anything */
/* returns 0 on success */
int strbuf_chrbuf(strbuf *d, const strbuf* s, char c);
int strbuf_rchrbuf(strbuf *d, const strbuf* s, char c);

/* basically strcmp */
int strbuf_cmp(const strbuf* s1, const strbuf* s2);

int strbuf_equals(const strbuf* s1, const strbuf* s2);
int strbuf_begins(const strbuf* s, const strbuf* q);
int strbuf_ends(const strbuf* s, const strbuf* q);

int strbuf_equals_cstr(const strbuf* s1, const char* s2);
int strbuf_begins_cstr(const strbuf* s, const char* q);
int strbuf_ends_cstr(const strbuf* s, const char* q);

/* strcasecmp */
int strbuf_casecmp(const strbuf* s1, const strbuf* s2);

int strbuf_caseequals(const strbuf* s1, const strbuf* s2);
int strbuf_casebegins(const strbuf* s, const strbuf* q);
int strbuf_caseends(const strbuf* s, const strbuf* q);

/* lazy wrapper for dealing with regular strings */
int strbuf_equal(const strbuf* s, const char* q, size_t len);
int strbuf_begin(const strbuf* s, const char* q, size_t len);
int strbuf_end(const strbuf* s, const char* q, size_t len);

long               strbuf_strtol(const strbuf*, int base);
long long          strbuf_strtoll(const strbuf*, int base);
unsigned long      strbuf_strtoul(const strbuf*, int base);
unsigned long long strbuf_strtoull(const strbuf*, int base);
double             strbuf_strtod(const strbuf*);
float              strbuf_strtof(const strbuf*);

/* windows only - copy a UTF-8 strbuf into a wchar_t buf */
#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
int strbuf_wide(strbuf* d, const strbuf* s);
int strbuf_wideterm(strbuf* d);
#endif

void strbuf_lower(strbuf*);
void strbuf_upper(strbuf*);

/* returns 1 if strbuf has something truth-y like "1", "yes", "on", "enable", "true" */
int strbuf_truthy(const strbuf*);

/* returns 0 if strbuf has something false-y like "0", "no", "off", "disable", "false" */
int strbuf_falsey(const strbuf*);

#endif
