/* SPDX-License-Identifier: Zlib */
#ifndef UAPI_VEX_UTSNAME_H
#define UAPI_VEX_UTSNAME_H

#define _k_SYS_NMLN 128

#define _k_utsname_s {          \
    char sysname[_k_SYS_NMLN];  \
    char nodename[_k_SYS_NMLN]; \
    char release[_k_SYS_NMLN];  \
    char version[_k_SYS_NMLN];  \
    char machine[_k_SYS_NMLN];  \
} /* _k_utsname_s */

#endif /* UAPI_VEX_UTSNAME_H */
