#ifndef CF91645C_FD7B_4867_ACAF_DFEFD227B222
#define CF91645C_FD7B_4867_ACAF_DFEFD227B222
#pragma once

#include <core/cdefs.h>
#include <core/types.h>
#include <stdarg.h>

#ifndef KP_BUFSIZE
#define KP_BUFSIZE 4096U
#endif

#if KP_BUFSIZE < 256U
#undef KP_BUFSIZE
#define KP_BUFSIZE 256U
#endif

#define KP_EMERG    0U ///< The system is unusable, absolutely fatal condition
#define KP_ALERT    1U ///< Action must be taken immediately, usually unrecoverable
#define KP_CRITICAL 2U ///< Critical conditions, probably unrecoverable
#define KP_ERROR    3U ///< Error conditions, probably recoverable
#define KP_WARNING  4U ///< Warning messages, definitely recoverable
#define KP_NOTICE   5U ///< Normal but important messages
#define KP_INFORM   6U ///< Informational messages
#define KP_VERBOSE  7U ///< Debug-level messages

extern size_t kp_writepos;
extern char kp_buffer[KP_BUFSIZE];
extern unsigned loglevel;

void kprintf(unsigned sv, const char* restrict fmt, ...) __printflike(2, 3);
void kvprintf(unsigned sv, const char* restrict fmt, va_list ap) __printflike(2, 0);

#define pr_emerg(fmt, ...)    kprintf(KP_EMERG, (fmt), ##__VA_ARGS__)
#define pr_alert(fmt, ...)    kprintf(KP_ALERT, (fmt), ##__VA_ARGS__)
#define pr_critical(fmt, ...) kprintf(KP_CRITICAL, (fmt), ##__VA_ARGS__)
#define pr_error(fmt, ...)    kprintf(KP_ERROR, (fmt), ##__VA_ARGS__)
#define pr_warning(fmt, ...)  kprintf(KP_WARNING, (fmt), ##__VA_ARGS__)
#define pr_notice(fmt, ...)   kprintf(KP_NOTICE, (fmt), ##__VA_ARGS__)
#define pr_inform(fmt, ...)   kprintf(KP_INFORM, (fmt), ##__VA_ARGS__)
#define pr_verbose(fmt, ...)  kprintf(KP_VERBOSE, (fmt), ##__VA_ARGS__)

#endif /* CF91645C_FD7B_4867_ACAF_DFEFD227B222 */
