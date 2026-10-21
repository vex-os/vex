#pragma once

#include <stdarg.h>
#include <vex/compiler.h>
#include <vex/event.h>
#include <vex/kplog.h>
#include <vex/types.h>

#define KP_BUFSIZE  ((unsigned)(CONFIG_KPRINTF_BUFSIZE))
#define KP_LINE_MAX 512U // Text bytes a record holds; a longer message is cut short

#define KP_EMERG    ((unsigned)(KPLOG_EMERG))
#define KP_ALERT    ((unsigned)(KPLOG_ALERT))
#define KP_CRITICAL ((unsigned)(KPLOG_CRITICAL))
#define KP_ERROR    ((unsigned)(KPLOG_ERROR))
#define KP_WARNING  ((unsigned)(KPLOG_WARNING))
#define KP_NOTICE   ((unsigned)(KPLOG_NOTICE))
#define KP_INFORM   ((unsigned)(KPLOG_INFORM))
#define KP_VERBOSE  ((unsigned)(KPLOG_VERBOSE))

typedef struct kp_cursor_s kp_cursor_t;

struct kp_cursor_s {
    uint64_t seq;
    uint64_t position; // Byte position of the record numbered seq, while the ring holds it
};

extern unsigned loglevel;
extern event_t kprintf_appended; // Triggered from a scheduler tick once records were appended

void kprintf(unsigned sv, const char* restrict fmt, ...) __printflike(2, 3);
void kvprintf(unsigned sv, const char* restrict fmt, va_list ap) __printflike(2, 0);

/// Points the cursor at the oldest record the ring holds
void kprintf_seek_first(kp_cursor_t* restrict cursor);

/// Points the cursor past the newest record, at the next one to be written
void kprintf_seek_end(kp_cursor_t* restrict cursor);

/// The sequence number the next record gets
uint64_t kprintf_next_seq(void) __nodiscard;

/// Copies the record under the cursor and moves the cursor past it
/// @param text Holds at least KP_LINE_MAX bytes
/// @retval false The cursor is past the newest record
/// @note A cursor whose record the ring dropped moves to the oldest one first,
/// so record->seq tells the caller how many it missed
bool kprintf_read(kp_cursor_t* restrict cursor, kplog_record_t* restrict record, char* restrict text);

#define pr_emerg(fmt, ...)    kprintf(KP_EMERG, (fmt), ##__VA_ARGS__)
#define pr_alert(fmt, ...)    kprintf(KP_ALERT, (fmt), ##__VA_ARGS__)
#define pr_critical(fmt, ...) kprintf(KP_CRITICAL, (fmt), ##__VA_ARGS__)
#define pr_error(fmt, ...)    kprintf(KP_ERROR, (fmt), ##__VA_ARGS__)
#define pr_warning(fmt, ...)  kprintf(KP_WARNING, (fmt), ##__VA_ARGS__)
#define pr_notice(fmt, ...)   kprintf(KP_NOTICE, (fmt), ##__VA_ARGS__)
#define pr_inform(fmt, ...)   kprintf(KP_INFORM, (fmt), ##__VA_ARGS__)
#define pr_verbose(fmt, ...)  kprintf(KP_VERBOSE, (fmt), ##__VA_ARGS__)
