/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_VEX_UTSNAME_H
#define INCLUDE_VEX_UTSNAME_H
#include <uapi/vex/utsname.h>

struct utsname _k_utsname_s;

extern const char sysname[];
extern const char release[];
extern const char version[];

#endif /* INCLUDE_VEX_UTSNAME_H */
