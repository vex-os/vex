/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef INCLUDE_VEX_UTSNAME_H
#define INCLUDE_VEX_UTSNAME_H

#if !defined(UNAME_SZ)
#define UNAME_SZ 128
#endif

struct utsname {
    char sysname[UNAME_SZ];
    char nodename[UNAME_SZ];
    char release[UNAME_SZ];
    char version[UNAME_SZ];
    char machine[UNAME_SZ];
};

#endif /* INCLUDE_VEX_UTSNAME_H */
