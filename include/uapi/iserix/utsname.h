/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_UAPI_ISERIX_UTSNAME_H
#define INCLUDE_UAPI_ISERIX_UTSNAME_H

#define _k_utsname_s {  \
    char sysname[64];   \
    char nodename[64];  \
    char release[64];   \
    char version[64];   \
    char machine[64];   \
} /* _k_utsname_s */

#endif /* INCLUDE_UAPI_ISERIX_UTSNAME_H */
