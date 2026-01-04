#ifndef _VEX_KPRINTF_H
#define _VEX_KPRINTF_H 1

#include <vex/compiler.h>

#include <stdarg.h>
#include <stddef.h>

#ifndef CONF_KPRINTF_BUFSIZE
#define CONF_KPRINTF_BUFSIZE 4086
#endif

#define KP_EMERG    0U ///< The system is unusable, absolutely fatal condition
#define KP_ALERT    1U ///< Action must be taken immediately, usually unrecoverable
#define KP_CRITICAL 2U ///< Critical conditions, probably unrecoverable
#define KP_ERROR    3U ///< Error conditions, probably recoverable
#define KP_WARNING  4U ///< Warning messages, definitely recoverable
#define KP_NOTICE   5U ///< Normal but important messages
#define KP_INFORM   6U ///< Informational messages
#define KP_DEBUG    7U ///< Debug-level messages

extern size_t kprintf_writepos;
extern char kprintf_buffer[CONF_KPRINTF_BUFSIZE];
extern unsigned int loglevel;

extern void kprintf(unsigned int sv, const char* restrict fmt, ...) __printflike(2, 3);
extern void kvprintf(unsigned int sv, const char* restrict fmt, va_list ap) __printflike(2, 0);

#endif
