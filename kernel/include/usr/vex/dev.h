#ifndef USR_VEX_DEV_H
#define USR_VEX_DEV_H

#include <vex/types.h>

/* A device number is its name taken apart: ada0n1p2 is crc32("ada"), unit 0, index 1, partition 2 */

#define __k_DEV_NO_INDEX     ((__k_uint32_t)(0xFFF))
#define __k_DEV_NO_PARTITION ((__k_uint32_t)(0xFF))

#define __k_DEV_SET_PREFIX(hash)         (((__k_dev_t)(hash) & 0xFFFFFFFF) << 32)
#define __k_DEV_SET_UNIT(unit)           (((__k_dev_t)(unit) & 0xFFF) << 20)
#define __k_DEV_SET_INDEX(index)         (((__k_dev_t)(index) & 0xFFF) << 8)
#define __k_DEV_SET_PARTITION(partition) (((__k_dev_t)(partition) & 0xFF))
#define __k_DEV_SET(hash, unit, index, partition) \
    (__k_DEV_SET_PREFIX(hash) | __k_DEV_SET_UNIT(unit) | __k_DEV_SET_INDEX(index) | __k_DEV_SET_PARTITION(partition))

#define __k_DEV_GET_PREFIX(dev)    ((__k_uint32_t)((__k_dev_t)(dev) >> 32))
#define __k_DEV_GET_UNIT(dev)      ((__k_uint32_t)((__k_dev_t)(dev) >> 20) & 0xFFF)
#define __k_DEV_GET_INDEX(dev)     ((__k_uint32_t)((__k_dev_t)(dev) >> 8) & 0xFFF)
#define __k_DEV_GET_PARTITION(dev) ((__k_uint32_t)(dev) & 0xFF)

/* What major() and minor() give */
#define __k_DEV_GET_MAJOR(dev) __k_DEV_GET_PREFIX(dev)
#define __k_DEV_GET_MINOR(dev) ((__k_uint32_t)((__k_dev_t)(dev) & 0xFFFFFFFF))

#endif /* USR_VEX_DEV_H */
