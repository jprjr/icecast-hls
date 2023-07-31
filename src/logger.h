#ifndef LOGGER_H
#define LOGGER_H

enum LOG_LEVEL {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
};

#ifdef __cpluspls
extern "C" {
#endif

/* initializes the global lock around stderr */
void logger_init(void);

/* destroys the global lock around stderr */
void logger_deinit(void);

/* enable/disable the use of color */
void logger_set_color(int enable);

/* enable/disable show per-message timestamps */
void logger_set_time(int enable);

/* enable/disable showing the source code info */
void logger_set_default_fileinfo(int enable);

/* set the default log level */
void logger_set_default_level(enum LOG_LEVEL level);

/* initializes thread-local storage, needs to be called in main */
int logger_tls_init(void);

/* de-initializes thread-local storage */
void logger_tls_deinit(void);

/* sets the per-thread prefix */
int logger_set_prefix(const char *prefix, size_t len);
int logger_append_prefix(const char *prefix, size_t len);

/* sets the per-thread log level */
int logger_set_level(enum LOG_LEVEL level);

/* sets the per-thread log fileinfo */
int logger_set_fileinfo(int enable);

/* clears data allocated by the thread */
void logger_thread_cleanup(void);

void logger_log(enum LOG_LEVEL level, const char *file, int line, const char *fmt, ...);

#ifdef LOG_PREFIX

#define log_trace(fmt,...) logger_log(LOG_TRACE, __FILE__, __LINE__, LOG_PREFIX " " fmt, __VA_ARGS__)
#define log_debug(fmt,...) logger_log(LOG_DEBUG, __FILE__, __LINE__, LOG_PREFIX " " fmt, __VA_ARGS__)
#define log_info(fmt,...)  logger_log(LOG_INFO,  __FILE__, __LINE__, LOG_PREFIX " " fmt, __VA_ARGS__)
#define log_warn(fmt,...)  logger_log(LOG_WARN,  __FILE__, __LINE__, LOG_PREFIX " " fmt, __VA_ARGS__)
#define log_error(fmt,...) logger_log(LOG_ERROR, __FILE__, __LINE__, LOG_PREFIX " " fmt, __VA_ARGS__)
#define log_fatal(fmt,...) logger_log(LOG_FATAL, __FILE__, __LINE__, LOG_PREFIX " " fmt, __VA_ARGS__)

#define logs_trace(str) logger_log(LOG_TRACE, __FILE__, __LINE__, LOG_PREFIX " " str)
#define logs_debug(str) logger_log(LOG_DEBUG, __FILE__, __LINE__, LOG_PREFIX " " str)
#define logs_info(str)  logger_log(LOG_INFO,  __FILE__, __LINE__, LOG_PREFIX " " str)
#define logs_warn(str)  logger_log(LOG_WARN,  __FILE__, __LINE__, LOG_PREFIX " " str)
#define logs_error(str) logger_log(LOG_ERROR, __FILE__, __LINE__, LOG_PREFIX " " str)
#define logs_fatal(str) logger_log(LOG_FATAL, __FILE__, __LINE__, LOG_PREFIX " " str)

#else

#define log_trace(fmt,...) logger_log(LOG_TRACE, __FILE__, __LINE__, fmt, __VA_ARGS__)
#define log_debug(fmt,...) logger_log(LOG_DEBUG, __FILE__, __LINE__, fmt, __VA_ARGS__)
#define log_info(fmt,...)  logger_log(LOG_INFO,  __FILE__, __LINE__, fmt, __VA_ARGS__)
#define log_warn(fmt,...)  logger_log(LOG_WARN,  __FILE__, __LINE__, fmt, __VA_ARGS__)
#define log_error(fmt,...) logger_log(LOG_ERROR, __FILE__, __LINE__, fmt, __VA_ARGS__)
#define log_fatal(fmt,...) logger_log(LOG_FATAL, __FILE__, __LINE__, fmt, __VA_ARGS__)

#define logs_trace(str) logger_log(LOG_TRACE, __FILE__, __LINE__, str)
#define logs_debug(str) logger_log(LOG_DEBUG, __FILE__, __LINE__, str)
#define logs_info(str)  logger_log(LOG_INFO,  __FILE__, __LINE__, str)
#define logs_warn(str)  logger_log(LOG_WARN,  __FILE__, __LINE__, str)
#define logs_error(str) logger_log(LOG_ERROR, __FILE__, __LINE__, str)
#define logs_fatal(str) logger_log(LOG_FATAL, __FILE__, __LINE__, str)

#endif

#ifdef __cplusplus
}
#endif

#endif
