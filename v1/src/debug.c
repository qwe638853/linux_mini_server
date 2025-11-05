#include "../include/debug.h"

// Runtime debug log control
static int g_debug_enabled = 0;
static log_level_t g_log_level = LOG_INFO;

void debug_log_enable(void) {
    g_debug_enabled = 1;
    #ifdef DEBUG
        // Compile-time debug support is enabled, runtime control will work
    #else
        // Warn user that compile-time debug support is not enabled
        // INFO_LOG, WARN_LOG, DEBUG_LOG are compiled as no-ops without DEBUG
        fprintf(stderr, "[WARNING] Runtime debug log enabled, but compile-time DEBUG support is disabled.\n");
        fprintf(stderr, "[WARNING] INFO_LOG, WARN_LOG, DEBUG_LOG will not output anything.\n");
        fprintf(stderr, "[WARNING] Only ERROR_LOG will work. Recompile with -DBUILD_DEBUG=ON to enable.\n");
    #endif
}

void debug_log_disable(void) {
    g_debug_enabled = 0;
}

int debug_log_is_enabled(void) {
    return g_debug_enabled;
}

void debug_log_set_level(log_level_t level) {
    if (level >= LOG_ERROR && level <= LOG_DEBUG) {
        g_log_level = level;
    }
}

log_level_t debug_log_get_level(void) {
    return g_log_level;
}

