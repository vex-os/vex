/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef AB619E39_F4DB_49DB_A29B_9BFE8CDB266A
#define AB619E39_F4DB_49DB_A29B_9BFE8CDB266A

#ifndef __k_UNAME_SIZE
#define __k_UNAME_SIZE 128
#endif

struct __k_utsname {
    char sysname[__k_UNAME_SIZE];
    char nodename[__k_UNAME_SIZE];
    char release[__k_UNAME_SIZE];
    char version[__k_UNAME_SIZE];
    char machine[__k_UNAME_SIZE];
};

#endif /* AB619E39_F4DB_49DB_A29B_9BFE8CDB266A */
