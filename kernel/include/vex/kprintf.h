#pragma once

#include <stdarg.h>
#include <vex/compiler.h>
#include <vex/types.h>

#define KP_BUFSIZE  ((unsigned)(CONFIG_KPRINTF_BUFSIZE))
#define KP_LINE_MAX 512U // Text bytes a record holds; a longer message is cut short

#define KP_EMERG    0U // The system is unusable, absolutely fatal condition
#define KP_ALERT    1U // Action must be taken immediately, usually unrecoverable
#define KP_CRITICAL 2U // Critical conditions, probably unrecoverable
#define KP_ERROR    3U // Error conditions, probably recoverable
#define KP_WARNING  4U // Warning messages, definitely recoverable
#define KP_NOTICE   5U // Normal but important messages
#define KP_INFORM   6U // Informational messages
#define KP_VERBOSE  7U // Debug-level messages

#define KP_RECORD_TRUNCATED 0x01U

typedef struct kp_record_s kp_record_t;
typedef struct kp_cursor_s kp_cursor_t;

struct kp_record_s {
    uint64_t seq;
    uint64_t time_ns; // On the clock_source_now_ns scale
    uint16_t size;    // Of the text that follows, with no trailing newline
    uint8_t level;
    uint8_t flags;
};

struct kp_cursor_s {
    uint64_t seq;
    uint64_t position; // Byte position of the record numbered seq, while the ring holds it
};

extern unsigned loglevel;

void kprintf(unsigned sv, const char* restrict fmt, ...) __printflike(2, 3);
void kvprintf(unsigned sv, const char* restrict fmt, va_list ap) __printflike(2, 0);

/// Points the cursor at the oldest record the ring holds
void kprintf_seek_first(kp_cursor_t* restrict cursor);

/// Points the cursor past the newest record, at the next one to be written
void kprintf_seek_end(kp_cursor_t* restrict cursor);

/// Copies the record under the cursor and moves the cursor past it
/// @param text Holds at least KP_LINE_MAX bytes
/// @retval false The cursor is past the newest record
/// @note A cursor whose record the ring dropped moves to the oldest one first,
/// so record->seq tells the caller how many it missed
bool kprintf_read(kp_cursor_t* restrict cursor, kp_record_t* restrict record, char* restrict text);

#define pr_emerg(fmt, ...)    kprintf(KP_EMERG, (fmt), ##__VA_ARGS__)
#define pr_alert(fmt, ...)    kprintf(KP_ALERT, (fmt), ##__VA_ARGS__)
#define pr_critical(fmt, ...) kprintf(KP_CRITICAL, (fmt), ##__VA_ARGS__)
#define pr_error(fmt, ...)    kprintf(KP_ERROR, (fmt), ##__VA_ARGS__)
#define pr_warning(fmt, ...)  kprintf(KP_WARNING, (fmt), ##__VA_ARGS__)
#define pr_notice(fmt, ...)   kprintf(KP_NOTICE, (fmt), ##__VA_ARGS__)
#define pr_inform(fmt, ...)   kprintf(KP_INFORM, (fmt), ##__VA_ARGS__)
#define pr_verbose(fmt, ...)  kprintf(KP_VERBOSE, (fmt), ##__VA_ARGS__)
