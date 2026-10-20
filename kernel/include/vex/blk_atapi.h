#pragma once

#include <vex/blk_device.h>

#define ATAPI_FLAG_WRITABLE 0x0001 // Device supports write commands

typedef struct atapi_device_s atapi_device_t;
typedef struct atapi_device_ops_s atapi_device_ops_t;

typedef uint8_t atapi_cdb_t[12];

struct atapi_device_ops_s {
    void (*submit_packet)(const atapi_device_t* restrict device, const atapi_cdb_t cdb, blk_request_t* restrict req);
    void (*release)(const atapi_device_t* restrict device);
};

struct atapi_device_s {
    blk_device_t base;
    const atapi_device_ops_t* ops;
    unsigned flags;
    void* ctxdata;
};

#define ATAPI_DEVICE_FROM_BLK(device) ((atapi_device_t*)(device))

int atapi_device_add(atapi_device_t* restrict device, blk_name_t* restrict name, unsigned iterator, unsigned index);
void atapi_device_remove(atapi_device_t* restrict device);
