#ifndef USR_VEX_KMSG_H
#define USR_VEX_KMSG_H

/* /dev/kmsg gives one record per line, as Linux does: "<prefix>,<seq>,<usec>,-;<text>"
 * The prefix is facility << 3 | level. In the text, a backslash and each byte outside
 * 0x20..0x7E is \xNN. A write is one record. A leading <N> sets its level */

#define __k_KMSG_EMERG    0 /* The system is unusable, absolutely fatal condition */
#define __k_KMSG_ALERT    1 /* Action must be taken immediately, usually unrecoverable */
#define __k_KMSG_CRITICAL 2 /* Critical conditions, probably unrecoverable */
#define __k_KMSG_ERROR    3 /* Error conditions, probably recoverable */
#define __k_KMSG_WARNING  4 /* Warning messages, definitely recoverable */
#define __k_KMSG_NOTICE   5 /* Normal but important messages */
#define __k_KMSG_INFORM   6 /* Informational messages */
#define __k_KMSG_VERBOSE  7 /* Debug-level messages */

#define __k_KMSG_FACILITY_KERNEL 0
#define __k_KMSG_FACILITY_USER   1 /* Records written to /dev/kmsg */

#endif /* USR_VEX_KMSG_H */
