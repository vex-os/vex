#ifndef USR_VEX_FEED_H
#define USR_VEX_FEED_H

#include <vex/types.h>

/* A feed is a virtual devfs node that gives records of kernel events
 * such as /dev/kplog for kprintf and /dev/hwtree for hardware updates.
 * Each read call gives only whole records and fails with EINVAL whenever
 * the next record doesn't fit in the user-provided buffer. Each record
 * starts with a common header and its size is always a multiple of __k_FEED_ALIGN */

#define __k_FEED_ALIGN 8

#define __k_FEED_TYPE_LOST   ((__k_uint16_t)(0x0001)) /* The feed dropped records before this one */
#define __k_FEED_TYPE_DEF(n) ((__k_uint16_t)(0x0100 + (n)))

struct __k_feed_header {
    __k_uint32_t size;  /* Of the whole record, header and padding included; a multiple of __k_FEED_ALIGN */
    __k_uint16_t type;  /* Never zero */
    __k_uint16_t flags; /* Their meaning depends on the type */
    __k_uint64_t seq;
    __k_uint64_t time_ns; /* On __k_CLOCK_MONOTONIC */
};

struct __k_feed_lost {
    struct __k_feed_header feed;
    __k_uint64_t count; /* Of the records dropped */
};

#endif /* USR_VEX_FEED_H */
