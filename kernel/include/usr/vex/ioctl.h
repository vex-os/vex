#ifndef USR_VEX_IOCTL_H
#define USR_VEX_IOCTL_H

#include <vex/types.h>

#define __k_IOCTL_DIR_VOID  ((__k_uint32_t)(0x00))
#define __k_IOCTL_DIR_WRITE ((__k_uint32_t)(0x01))
#define __k_IOCTL_DIR_READ  ((__k_uint32_t)(0x02))
#define __k_IOCTL_DIR_RDWR  ((__k_uint32_t)(0x03))

#define __k_IOCTL_SET_DIR(dir)     (((__k_uint32_t)(dir) & 0x03) << 30)
#define __k_IOCTL_SET_SIZE(size)   (((__k_uint32_t)(size) & 0x3FFF) << 16)
#define __k_IOCTL_SET_GROUP(group) (((__k_uint32_t)(group) & 0xFF) << 8)
#define __k_IOCTL_SET_CMD(cmd)     (((__k_uint32_t)(cmd) & 0xFF))
#define __k_IOCTL_SET(dir, size, group, cmd) \
    (__k_IOCTL_SET_DIR(dir) | __k_IOCTL_SET_SIZE(size) | __k_IOCTL_SET_GROUP(group) | __k_IOCTL_SET_CMD(cmd))

#define __k_IOCTL_VOID(group, cmd)        __k_IOCTL_SET(__k_IOCTL_DIR_VOID, (0), (group), (cmd))
#define __k_IOCTL_READ(size, group, cmd)  __k_IOCTL_SET(__k_IOCTL_DIR_READ, (size), (group), (cmd))
#define __k_IOCTL_WRITE(size, group, cmd) __k_IOCTL_SET(__k_IOCTL_DIR_WRITE, (size), (group), (cmd))
#define __k_IOCTL_RDWR(size, group, cmd)  __k_IOCTL_SET(__k_IOCTL_DIR_RDWR, (size), (group), (cmd))

#define __k_IOCTL_GET_DIR(request)   (((__k_uint32_t)(request) >> 30) & 0x3)
#define __k_IOCTL_GET_SIZE(request)  (((__k_uint32_t)(request) >> 16) & 0x3FFF)
#define __k_IOCTL_GET_GROUP(request) (((__k_uint32_t)(request) >> 8) & 0xFF)
#define __k_IOCTL_GET_CMD(request)   ((__k_uint32_t)(request) & 0xFF)

#endif /* USR_VEX_IOCTL_H */
