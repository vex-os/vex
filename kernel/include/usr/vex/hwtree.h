#ifndef USR_VEX_HWTREE_H
#define USR_VEX_HWTREE_H

#include <vex/feed.h>
#include <vex/ioctl.h>

#define __k_HWTREE_STATE_PRESENT   0 /* In the tree, without a driver */
#define __k_HWTREE_STATE_PROBING   1 /* Drivers are scored, and the best attaches */
#define __k_HWTREE_STATE_WAITING   2 /* The best driver waits for a provider to be bound */
#define __k_HWTREE_STATE_BOUND     3 /* A driver has it */
#define __k_HWTREE_STATE_DETACHING 4 /* The driver releases it */
#define __k_HWTREE_STATE_GONE      5 /* Out of the tree */

/* A reader gets the tree as it is at open between SNAPSHOT_BEGIN and SNAPSHOT_END,
 * parents before their children, then the changes. After __k_FEED_TYPE_LOST comes a new snapshot */
#define __k_FEED_TYPE_HWTREE_SNAPSHOT_BEGIN   __k_FEED_TYPE_DEF(0)
#define __k_FEED_TYPE_HWTREE_SNAPSHOT_END     __k_FEED_TYPE_DEF(1)
#define __k_FEED_TYPE_HWTREE_ADD              __k_FEED_TYPE_DEF(2) /* Fields: name, path, driver, fwid, and those of the bus */
#define __k_FEED_TYPE_HWTREE_REMOVE           __k_FEED_TYPE_DEF(3)
#define __k_FEED_TYPE_HWTREE_BIND             __k_FEED_TYPE_DEF(4) /* Fields: driver */
#define __k_FEED_TYPE_HWTREE_UNBIND           __k_FEED_TYPE_DEF(5) /* Fields: driver */
#define __k_FEED_TYPE_HWTREE_WAITING          __k_FEED_TYPE_DEF(6) /* Fields: driver, then for or for_name */
#define __k_FEED_TYPE_HWTREE_ATTACH_FAILED    __k_FEED_TYPE_DEF(7) /* Fields: driver, error */
#define __k_FEED_TYPE_HWTREE_INTERFACE        __k_FEED_TYPE_DEF(8) /* Fields: kind (block or char), name */
#define __k_FEED_TYPE_HWTREE_INTERFACE_REMOVE __k_FEED_TYPE_DEF(9) /* Fields: name */

#define __k_HWTREE_FLAG_TRUNCATED 0x0001 /* Some fields didn't fit and were left out whole */

#define __k_HWTREE_FIELDS_MAX 512U

struct __k_hwtree_record {
    struct __k_feed_header feed;
    __k_uint64_t id;     /* Of the node; zero for SNAPSHOT_BEGIN and SNAPSHOT_END */
    __k_uint64_t parent; /* Zero for a child of the root */
    __k_uint32_t state;  /* Value of one of __k_HWTREE_STATE, at the time of the record */
    __k_uint32_t fields_size;
    char fields[__k_HWTREE_FIELDS_MAX]; /* Zero-separated key=value pairs */
};

/* Commands that change the tree take a file opened for writing */
#define __k_HWTREE_RESNAPSHOT __k_IOCTL_VOID('H', 0x01)
#define __k_HWTREE_RESCAN     __k_IOCTL_WRITE(sizeof(__k_uint64_t), 'H', 0x02)
#define __k_HWTREE_UNBIND     __k_IOCTL_WRITE(sizeof(__k_uint64_t), 'H', 0x03)
#define __k_HWTREE_EJECT      __k_IOCTL_WRITE(sizeof(__k_uint64_t), 'H', 0x04)

#endif /* USR_VEX_HWTREE_H */
