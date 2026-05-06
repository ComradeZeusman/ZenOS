#ifndef KLOG_H
#define KLOG_H

#include "kernel_types.h"

/*
 * Kernel logging levels, in ascending severity order.
 */
typedef enum {
    LOG_DEBUG = 0,   /* verbose tracing, normally compiled in but silent     */
    LOG_INFO  = 1,   /* normal operational messages (component init, etc.)   */
    LOG_WARN  = 2,   /* unexpected but recoverable conditions                */
    LOG_ERROR = 3,   /* serious errors; kernel may still continue            */
} log_level_t;

/*
 * klog – emit a single-line log message.
 *
 * Prepends a colour-coded level tag:
 *   [DEBUG]   dark grey
 *   [ INFO]   light green
 *   [ WARN]   yellow
 *   [ERROR]   light red
 *
 * Appends a newline automatically.
 */
void klog(log_level_t level, const char *msg);

/*
 * klog_hex – log a message followed by a 32-bit hex value.
 *
 * Example:
 *   klog_hex(LOG_INFO, "free frames = ", 42);
 * prints:
 *   [ INFO] free frames = 0x0000002A
 */
void klog_hex(log_level_t level, const char *prefix, uint32_t val);

/* Convenience macros (saves typing the level enum every time) */
#define KLOG_DEBUG(msg)  klog(LOG_DEBUG, (msg))
#define KLOG_INFO(msg)   klog(LOG_INFO,  (msg))
#define KLOG_WARN(msg)   klog(LOG_WARN,  (msg))
#define KLOG_ERROR(msg)  klog(LOG_ERROR, (msg))

#endif /* KLOG_H */
