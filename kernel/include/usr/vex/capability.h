#ifndef USR_VEX_CAPABILITY_H
#define USR_VEX_CAPABILITY_H

#include <vex/types.h>

/* NOTE: __k_CAP bit numbers are same as ones
 * defined in Linux, so that any file operations
 * done there transfer to Vex without issues */

#define __k_CAP_CHOWN        0
#define __k_CAP_DAC_OVERRIDE 1 /* Bypass file permission checks */
#define __k_CAP_FOWNER       3 /* Act as the owner of any file */
#define __k_CAP_FSETID       4 /* Keep setuid and setgid bits a change would clear */
#define __k_CAP_SETGID       6
#define __k_CAP_SETUID       7

/* Vex-only capabilities start at 48 */

#define __k_CAP_MOUNT 48

#define __k_CAP_TO_MASK(cap) ((__k_uint64_t)(1) << (cap))

#define __k_CAPS_KEEP 0x0001 /* Keep the permitted set when no uid is 0 anymore */

struct __k_caps {
    __k_uint64_t effective;
    __k_uint64_t permitted;
    __k_uint64_t ambient;  /* Survives exec of a program that is not setuid or setgid */
    __k_uint64_t bounding; /* No exec ever grants more than this */
    __k_uint32_t flags;
    __k_uint32_t reserved;
};

#endif /* USR_VEX_CAPABILITY_H */
