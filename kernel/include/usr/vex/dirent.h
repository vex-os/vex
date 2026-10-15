#ifndef USR_VEX_DIRENT_H
#define USR_VEX_DIRENT_H

#include <vex/types.h>

struct __k_dirent {
    __k_ino_t d_ino;
    __k_uint16_t d_nodetype;
    __k_uint8_t d_name[256];
};

#endif /* USR_VEX_DIRENT_H */
