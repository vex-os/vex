#ifndef USR_VEX_DIRENT_H
#define USR_VEX_DIRENT_H

#include <vex/types.h>

/* A record in the getdents buffer ends after d_name,
 * padded to 8 bytes; d_reclen is the step to the next one */
struct __k_dirent {
    __k_ino_t d_ino;
    __k_uint64_t d_off; /* Cookie of the next entry */
    __k_uint16_t d_reclen;
    __k_uint16_t d_nodetype;
    __k_uint8_t d_name[256];
};

#endif /* USR_VEX_DIRENT_H */
