#ifndef USR_VEX_FCNTL_H
#define USR_VEX_FCNTL_H

#include <vex/types.h>

#define __k_O_RDONLY   ((__k_mode_t)(0x0001))
#define __k_O_WRONLY   ((__k_mode_t)(0x0002))
#define __k_O_RDWR     ((__k_mode_t)(0x0003))
#define __k_O_CREAT    ((__k_mode_t)(0x0004))
#define __k_O_TRUNC    ((__k_mode_t)(0x0008))
#define __k_O_APPEND   ((__k_mode_t)(0x0010))
#define __k_O_EXCL     ((__k_mode_t)(0x0020))
#define __k_O_NONBLOCK ((__k_mode_t)(0x0040))

#define __k_F_DUPFD 0 /* TODO */
#define __k_F_GETFD 1 /* TODO */
#define __k_F_SETFD 2 /* TODO */
#define __k_F_GETFL 3
#define __k_F_SETFL 4

#define __k_SEEK_SET 0
#define __k_SEEK_CUR 1
#define __k_SEEK_END 2

#define __k_F_OK 0x0000
#define __k_X_OK 0x0001
#define __k_W_OK 0x0002
#define __k_R_OK 0x0004

#define __k_AT_FDCWD            (-100)
#define __k_AT_SYMLINK_NOFOLLOW 0x0100
#define __k_AT_REMOVEDIR        0x0200
#define __k_AT_EMPTY_PATH       0x0400
#define __k_AT_SYMLINK_FOLLOW   0x0800
#define __k_AT_EACCESS          0x1000 /* TODO */

#endif /* USR_VEX_FCNTL_H */
