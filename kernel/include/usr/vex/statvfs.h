#ifndef USR_VEX_STATVFS_H
#define USR_VEX_STATVFS_H

#include <vex/types.h>

#define __k_ST_RDONLY ((__k_uint32_t)(0x0001))
#define __k_ST_NOSUID ((__k_uint32_t)(0x0002))

struct __k_statvfs {
    __k_blksize_t f_bsize;
    __k_blksize_t f_frsize;
    __k_fsblkcnt_t f_blocks;
    __k_fsblkcnt_t f_bfree;
    __k_fsblkcnt_t f_bavail;
    __k_fsfilcnt_t f_files;
    __k_fsfilcnt_t f_ffree;
    __k_fsfilcnt_t f_favail;
    __k_uint32_t f_fsid;
    __k_uint32_t f_flag;
    __k_uint32_t f_namemax;
};

#endif /* USR_VEX_STATVFS_H */
