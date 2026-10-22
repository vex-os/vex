#pragma once

#include <vex/block_interface.h>

#define ATAPI_FLAG_WRITABLE 0x0001

typedef struct atapi_interface_s atapi_interface_t;
typedef struct atapi_interface_ops_s atapi_interface_ops_t;

typedef uint8_t atapi_cdb_t[12];

struct atapi_interface_ops_s {
    void (*submit_packet)(const atapi_interface_t* restrict interface, const atapi_cdb_t cdb, block_request_t* restrict req);
    void (*release)(const atapi_interface_t* restrict interface);
};

struct atapi_interface_s {
    block_interface_t base;
    const atapi_interface_ops_t* ops;
    unsigned flags;
    void* ctxdata;
};

#define ATAPI_INTERFACE_FROM_BLOCK(device) ((atapi_interface_t*)(device))

int atapi_interface_add(atapi_interface_t* restrict interface, dev_unit_t* restrict unit, unsigned index);
void atapi_interface_remove(atapi_interface_t* restrict interface);
