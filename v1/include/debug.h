#pragma once
#include <stdio.h>

// Log levels
typedef enum {
    LOG_ERROR = 0,
    LOG_WARN  = 1,
    LOG_INFO  = 2,
    LOG_DEBUG = 3
} log_level_t;

// Log control functions
void debug_log_enable(void);
void debug_log_disable(void);
int debug_log_is_enabled(void);
void debug_log_set_level(log_level_t level);
log_level_t debug_log_get_level(void);

// Log macros with levels
// ERROR: always enabled (critical issues)
#define ERROR_LOG(fp, fmt, ...) \
    fprintf(fp, "[ERROR] " fmt, ##__VA_ARGS__)

// WARN: requires DEBUG compilation and runtime enable
#ifdef DEBUG
#define WARN_LOG(fp, fmt, ...) \
    do { \
        if (debug_log_is_enabled() && debug_log_get_level() >= LOG_WARN) { \
            fprintf(fp, "[WARN] " fmt, ##__VA_ARGS__); \
        } \
    } while(0)
#else
#define WARN_LOG(fp, fmt, ...) ((void)0)
#endif

// INFO: requires DEBUG compilation and runtime enable
#ifdef DEBUG
#define INFO_LOG(fp, fmt, ...) \
    do { \
        if (debug_log_is_enabled() && debug_log_get_level() >= LOG_INFO) { \
            fprintf(fp, "[INFO] " fmt, ##__VA_ARGS__); \
        } \
    } while(0)
#else
#define INFO_LOG(fp, fmt, ...) ((void)0)
#endif

// DEBUG: requires DEBUG compilation and runtime enable
#ifdef DEBUG
#define DEBUG_LOG(fp, fmt, ...) \
    do { \
        if (debug_log_is_enabled() && debug_log_get_level() >= LOG_DEBUG) { \
            fprintf(fp, "[DEBUG] " fmt, ##__VA_ARGS__); \
        } \
    } while(0)
#else
#define DEBUG_LOG(fp, fmt, ...) ((void)0)
#endif

