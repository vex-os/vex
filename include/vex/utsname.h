/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_VEX_UTSNAME_H
#define INCLUDE_VEX_UTSNAME_H

struct utsname {
    char sysname[128];
    char nodename[128];
    char release[128];
    char version[128];
    char machine[128];
};

#endif /* INCLUDE_VEX_UTSNAME_H */
