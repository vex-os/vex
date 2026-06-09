#ifndef A3F9F255_18A7_4E12_B8B7_E0E4F6D47EEB
#define A3F9F255_18A7_4E12_B8B7_E0E4F6D47EEB
#pragma once

#include <sys/blk_device.h>

typedef uint8_t atapi_cdb_type[12];

struct atapi_device;
struct atapi_device_ops {
    int (*send_packet)(const struct atapi_device* restrict device, const atapi_cdb_type cdb, void* restrict buffer, size_t count);
};

#define ATAPI_FLAG_WRITABLE 0x0001 // Device supports write commands

struct atapi_device {
    struct blk_device base;
    const struct atapi_device_ops* ops;
    unsigned flags;
    void* ctxdata;
};

#define ATAPI_DEVICE_FROM_BLK(device) ((struct atapi_device*)(device))

int add_atapi_device(struct atapi_device* restrict device, struct blk_name* restrict name, unsigned iterator, unsigned index);
void remove_atapi_device(struct atapi_device* restrict device);

#endif /* A3F9F255_18A7_4E12_B8B7_E0E4F6D47EEB */
