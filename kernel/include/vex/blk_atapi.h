#pragma once

#include <vex/blk_device.h>

typedef uint8_t atapi_cdb_type[12];

struct atapi_device;
struct atapi_device_ops {
    int (*send_packet)(const struct atapi_device* restrict device, const atapi_cdb_type cdb, void* restrict buffer, size_t count);
    void (*release)(const struct atapi_device* restrict device);
};

#define ATAPI_FLAG_WRITABLE 0x0001 // Device supports write commands

struct atapi_device {
    struct blk_device base;
    const struct atapi_device_ops* ops;
    unsigned flags;
    size_t max_transfer; // Bytes per packet, zero if unlimited
    void* ctxdata;
};

#define ATAPI_DEVICE_FROM_BLK(device) ((struct atapi_device*)(device))

int add_atapi_device(struct atapi_device* restrict device, struct blk_name* restrict name, unsigned iterator, unsigned index);
void remove_atapi_device(struct atapi_device* restrict device);
