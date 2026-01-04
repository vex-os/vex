#ifndef _UAPI_VEX_UTSNAME_H
#define _UAPI_VEX_UTSNAME_H 1

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

#endif
