#ifndef USR_VEX_XATTR_H
#define USR_VEX_XATTR_H

/* These sit above every AT_* flag so both
 * share the flags argument of setxattrat */

#define __k_XATTR_CREATE  0x10000 /* Fail if the attribute exists */
#define __k_XATTR_REPLACE 0x20000 /* Fail if the attribute doesn't exist */

#define __k_XATTR_NAME_MAX 255
#define __k_XATTR_SIZE_MAX 65536
#define __k_XATTR_LIST_MAX 65536

#endif /* USR_VEX_XATTR_H */
