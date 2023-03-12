/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __USR_INCLUDE_KAN_UTSNAME_H__
#define __USR_INCLUDE_KAN_UTSNAME_H__

struct utsname {
    char sysname[128];
    char nodename[128];
    char release[128];
    char version[128];
    char machine[128];
};

#endif /* __USR_INCLUDE_KAN_UTSNAME_H__ */
