/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_UAPI_ISERIX_STATVFS_H
#define INCLUDE_UAPI_ISERIX_STATVFS_H
#include <iserix/types.h>

#define _k_ST_RDONLY 0x00000001UL
#define _k_ST_NOSUID 0x00000002UL

#define _k_statvfs_s {          \
    unsigned long f_bsize;      \
    unsigned long f_frsize;     \
    _k_fsblkcnt_t f_blocks;     \
    _k_fsblkcnt_t f_bfree;      \
    _k_fsblkcnt_t f_bavail;     \
    _k_fsfilcnt_t f_files;      \
    _k_fsfilcnt_t f_ffree;      \
    _k_fsfilcnt_t f_favail;     \
    unsigned long f_fsid;       \
    unsigned long f_flag;       \
    unsigned long f_namemax;    \
} /* _k_statvfs_s */

#endif /* INCLUDE_UAPI_ISERIX_STATVFS_H */
