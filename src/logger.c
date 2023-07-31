#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"
#include "thread.h"

static enum LOG_LEVEL default_log_level = LOG_WARN;
static int use_color = 1;
static int show_fileinfo = 0;

static const char *level_strings[] = {
    "TRACE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"
};

#define COLOR_RESET    "\x1b[0m"

#define COLOR_BLACK    "\x1b[30m"
#define COLOR_RED      "\x1b[31m"
#define COLOR_GREEN    "\x1b[32m"
#define COLOR_YELLOW   "\x1b[33m"
#define COLOR_BLUE     "\x1b[34m"
#define COLOR_MAGENTA  "\x1b[35m"
#define COLOR_CYAN     "\x1b[36m"
#define COLOR_WHITE    "\x1b[37m"

#define COLOR_BRIGHT_BLACK    "\x1b[90m"
#define COLOR_BRIGHT_RED      "\x1b[91m"
#define COLOR_BRIGHT_GREEN    "\x1b[92m"
#define COLOR_BRIGHT_YELLOW   "\x1b[93m"
#define COLOR_BRIGHT_BLUE     "\x1b[94m"
#define COLOR_BRIGHT_MAGENTA  "\x1b[95m"
#define COLOR_BRIGHT_CYAN     "\x1b[96m"
#define COLOR_BRIGHT_WHITE    "\x1b[97m"

static const char *level_colors[] = {
    /* TRACE */ COLOR_BRIGHT_CYAN,
    /* DEBUG */ COLOR_CYAN,
    /* INFO  */ COLOR_GREEN,
    /* WARN  */ COLOR_YELLOW,
    /* ERROR */ COLOR_RED,
    /* FATAL */ COLOR_MAGENTA,
};

struct logger_config {
    char *prefix;
    enum LOG_LEVEL level;
    int show_fileinfo;
};
typedef struct logger_config logger_config;

static thread_mutex_t stderr_mutex;
static thread_tls_t thread_config = NULL;

static void logger_stderr(enum LOG_LEVEL level, int use_fileinfo, const char *file, int line, const char *prefix, const char *fmt, va_list ap) {
    const char *color = "";
    const char *color_reset = "";
    if(use_color) {
        color = level_colors[level];
        color_reset = COLOR_RESET;
    }
    if(prefix == NULL) {
        prefix = "";
    }

    thread_mutex_lock(&stderr_mutex);
    if(use_fileinfo) {
        fprintf(stderr, "%s%-5s%s %s:%d: [%s] ",
          color, level_strings[level], color_reset,
          file, line, prefix);
    } else {
        fprintf(stderr, "%s%-5s%s [%s] ",
          color, level_strings[level], color_reset,
          prefix);
    }
    vfprintf(stderr,fmt,ap);
    fprintf(stderr,"\n");
    fflush(stderr);
    thread_mutex_unlock(&stderr_mutex);
    return;
}

static int logger_init_config(void) {
    logger_config *c = NULL;
    c = (logger_config*)malloc(sizeof(logger_config));
    if(c == NULL) return -1;
    c->prefix = NULL;
    c->level  = default_log_level;
    c->show_fileinfo = show_fileinfo;

    thread_tls_set(thread_config, c);
    return 0;
}

void logger_init(void) {
    thread_mutex_init(&stderr_mutex);
}

void logger_deinit(void) {
    thread_mutex_term(&stderr_mutex);
}

void logger_set_default_level(enum LOG_LEVEL level) {
    default_log_level = level;
}

int logger_tls_init(void) {
    thread_config = thread_tls_create();
    if(thread_config == NULL) return -1;
    return 0;
}

void logger_tls_deinit(void) {
    thread_tls_destroy(thread_config);
}

int logger_set_level(enum LOG_LEVEL level) {
    logger_config *config = NULL;
    int r;

    config = (logger_config *) thread_tls_get(thread_config);
    if(config == NULL) {
        r = logger_init_config();
        if(r != 0) return r;
        config = (logger_config *) thread_tls_get(thread_config);
    }

    config->level = level;
    return 0;
}

int logger_set_prefix(const char *prefix, size_t len) {
    logger_config *config = NULL;
    int r;

    config = (logger_config *) thread_tls_get(thread_config);
    if(config == NULL) {
        r = logger_init_config();
        if(r != 0) return r;
        config = (logger_config *) thread_tls_get(thread_config);
    }

    config->prefix = (char *)malloc(len + 1);
    if(config->prefix == NULL) return -1;
    memcpy(&config->prefix[0],prefix,len);
    config->prefix[len] = '\0';

    return 0;
}

int logger_append_prefix(const char *prefix, size_t len) {
    logger_config *config = NULL;
    int r;
    size_t olen = 0;

    config = (logger_config *) thread_tls_get(thread_config);
    if(config == NULL) {
        r = logger_init_config();
        if(r != 0) return r;
        config = (logger_config *) thread_tls_get(thread_config);
    }

    if(config->prefix != NULL) {
        olen = strlen(config->prefix);
    }

    config->prefix = (char *)realloc(config->prefix, olen + len + 1);
    if(config->prefix == NULL) return -1;
    memcpy(&config->prefix[olen],prefix,len);
    config->prefix[olen + len] = '\0';

    return 0;
}

void logger_thread_cleanup(void) {
    logger_config *config = NULL;
    config = (logger_config *) thread_tls_get(thread_config);
    if(config == NULL) return;

    if(config->prefix != NULL) free(config->prefix);
    free(config);
    thread_tls_set(thread_config, NULL);
}

void logger_log(enum LOG_LEVEL level, const char *file, int line, const char *fmt, ...) {
    va_list args_list;
    logger_config *config = NULL;

    va_start(args_list, fmt);

    if(thread_config == NULL) { /* we haven't initialized or it failed */
        logger_stderr(level, 1, file,line, NULL, fmt,args_list);
        goto cleanup;
    }

    config = (logger_config *)thread_tls_get(thread_config);
    if(config == NULL) {
        logger_stderr(level, 1, file,line, NULL, fmt,args_list);
        goto cleanup;
    }

    if(level >= config->level) {
        logger_stderr(level, config->show_fileinfo, file,line,config->prefix, fmt,args_list);
        goto cleanup;
    }

    cleanup:
    va_end(args_list);
    return;
}

void logger_set_color(int enable) {
    use_color = enable;
}
