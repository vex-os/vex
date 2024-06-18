/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_ISERIX_UTSNAME_H
#define INCLUDE_ISERIX_UTSNAME_H
#include <uapi/iserix/utsname.h>

struct utsname _k_utsname_s;

extern const char sysname[];
extern const char release[];
extern const char version[];
extern const char machine[];

#endif /* INCLUDE_ISERIX_UTSNAME_H */
