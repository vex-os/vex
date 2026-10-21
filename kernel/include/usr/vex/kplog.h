#ifndef USR_VEX_KPLOG_H
#define USR_VEX_KPLOG_H

#include <vex/types.h>

#define __k_KPLOG_EMERG    0 /* The system is unusable, absolutely fatal condition */
#define __k_KPLOG_ALERT    1 /* Action must be taken immediately, usually unrecoverable */
#define __k_KPLOG_CRITICAL 2 /* Critical conditions, probably unrecoverable */
#define __k_KPLOG_ERROR    3 /* Error conditions, probably recoverable */
#define __k_KPLOG_WARNING  4 /* Warning messages, definitely recoverable */
#define __k_KPLOG_NOTICE   5 /* Normal but important messages */
#define __k_KPLOG_INFORM   6 /* Informational messages */
#define __k_KPLOG_VERBOSE  7 /* Debug-level messages */

#define __k_KPLOG_TRUNCATED ((__k_uint8_t)(0x01)) /* The message was longer than a record holds */

/* A read gives one record, with its size bytes of text right after the structure */
struct __k_kplog_record {
    __k_uint64_t seq;
    __k_uint64_t time_ns; /* Since boot */
    __k_uint16_t size;    /* No trailing newline or NUL */
    __k_uint8_t level;
    __k_uint8_t flags;
};

#endif /* USR_VEX_KPLOG_H */
