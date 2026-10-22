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

/// Appends the text as one record, unformatted
/// @pre size is at most KP_LINE_MAX, and the text holds no line breaks
/// @param flags The KPLOG_* flags of the record
void kputs(unsigned sv, unsigned flags, const char* restrict text, size_t size);

void kprintf_seek_first(kp_cursor_t* restrict cursor);

void kprintf_seek_end(kp_cursor_t* restrict cursor);

uint64_t kprintf_next_seq(void) __nodiscard;

/// Copies the record under the cursor and moves the cursor past it
/// @param text Holds at least KP_LINE_MAX bytes
/// @retval false The cursor is past the newest record
/// @note If the ring dropped the record under the cursor, the cursor first moves to the oldest record,
/// so record->seq shows how many the caller missed
bool kprintf_read(kp_cursor_t* restrict cursor, kplog_record_t* restrict record, char* restrict text);

#define pr_emerg(fmt, ...)    kprintf(KP_EMERG, (fmt), ##__VA_ARGS__)
#define pr_alert(fmt, ...)    kprintf(KP_ALERT, (fmt), ##__VA_ARGS__)
#define pr_critical(fmt, ...) kprintf(KP_CRITICAL, (fmt), ##__VA_ARGS__)
#define pr_error(fmt, ...)    kprintf(KP_ERROR, (fmt), ##__VA_ARGS__)
#define pr_warning(fmt, ...)  kprintf(KP_WARNING, (fmt), ##__VA_ARGS__)
#define pr_notice(fmt, ...)   kprintf(KP_NOTICE, (fmt), ##__VA_ARGS__)
#define pr_inform(fmt, ...)   kprintf(KP_INFORM, (fmt), ##__VA_ARGS__)
#define pr_verbose(fmt, ...)  kprintf(KP_VERBOSE, (fmt), ##__VA_ARGS__)
