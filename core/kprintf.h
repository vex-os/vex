#ifndef CF91645C_FD7B_4867_ACAF_DFEFD227B222
#define CF91645C_FD7B_4867_ACAF_DFEFD227B222

#include <core/cdefs.h>

#include <stdarg.h>
#include <stddef.h>

#ifndef KP_BUFSIZE
#define KP_BUFSIZE 4096
#endif

#if KP_BUFSIZE < 256
#undef KP_BUFSIZE
#define KP_BUFSIZE 256
#endif

enum {
    KP_EMERG = 0,    ///< The system is unusable, absolutely fatal condition
    KP_ALERT = 1,    ///< Action must be taken immediately, usually unrecoverable
    KP_CRITICAL = 2, ///< Critical conditions, probably unrecoverable
    KP_ERROR = 3,    ///< Error conditions, probably recoverable
    KP_WARNING = 4,  ///< Warning messages, definitely recoverable
    KP_NOTICE = 5,   ///< Normal but important messages
    KP_INFORM = 6,   ///< Informational messages
    KP_DEBUG = 7,    ///< Debug-level messages
};

extern size_t kp_writepos;
extern char kp_buffer[KP_BUFSIZE];
extern unsigned int loglevel;

extern void kprintf(unsigned int sv, const char* restrict fmt, ...) __printflike(2, 3);
extern void kvprintf(unsigned int sv, const char* restrict fmt, va_list ap) __printflike(2, 0);

#endif /* CF91645C_FD7B_4867_ACAF_DFEFD227B222 */
