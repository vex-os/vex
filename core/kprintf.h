#ifndef CF91645C_FD7B_4867_ACAF_DFEFD227B222
#define CF91645C_FD7B_4867_ACAF_DFEFD227B222

#include <core/cdefs.h>
#include <stdarg.h>
#include <stddef.h>

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
#define KP_DEBUG    7U ///< Debug-level messages

extern size_t kp_writepos;
extern char kp_buffer[KP_BUFSIZE];
extern unsigned int loglevel;

extern void kprintf(unsigned int sv, const char* restrict fmt, ...) __printflike(2, 3);
extern void kvprintf(unsigned int sv, const char* restrict fmt, va_list ap) __printflike(2, 0);

#endif /* CF91645C_FD7B_4867_ACAF_DFEFD227B222 */
