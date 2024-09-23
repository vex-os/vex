/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef INCLUDE_VEX_STATVFS_H
#define INCLUDE_VEX_STATVFS_H
#include <vex/types.h>

#define ST_RDONLY 0x00000001UL
#define ST_NOSUID 0x00000002UL

struct statvfs {
    unsigned long f_bsize;
    unsigned long f_frsize;

    fsblkcnt_t f_blocks;
    fsblkcnt_t f_bfree;
    fsblkcnt_t f_bavail;

    fsfilcnt_t f_files;
    fsfilcnt_t f_ffree;
    fsfilcnt_t f_favail;

    unsigned long f_fsid;
    unsigned long f_flag;
    unsigned long f_namemax;
};

#endif /* INCLUDE_VEX_STATVFS_H */
