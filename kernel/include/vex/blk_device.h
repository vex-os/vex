#pragma once

#include <vex/compiler.h>
#include <vex/errno.h>
#include <vex/types.h>

typedef struct blk_name_s blk_name_t;

enum {
    BLK_REQ_READ,
    BLK_REQ_WRITE,
    BLK_REQ_FLUSH,
};

#define BLK_FLAG_PRESENT   0x0001 // Media is present and ready
#define BLK_FLAG_PARTITION 0x0002
#define BLK_FLAG_HIDDEN    0x0004 // Device is not added to devfs

typedef struct blk_segment_s blk_segment_t;
typedef struct blk_request_s blk_request_t;
typedef struct blk_device_s blk_device_t;
typedef struct blk_device_ops_s blk_device_ops_t;
typedef struct blk_device_private_s blk_device_private_t;

struct blk_segment_s {
    uintptr_t phys;
    size_t length;
};

/// Called once per request with status set, possibly before blk_submit returns
typedef void (*blk_request_done_t)(blk_request_t* restrict req);

struct blk_request_s {
    unsigned op;
    uintptr_t lba;
    size_t length;
    const blk_segment_t* segments;
    size_t num_segments;
    int status;
    blk_request_done_t done;
    void* ctxdata;
};

struct blk_device_ops_s {
    /// @pre blk_submit has checked the request against the device's limits and found the device present
    /// @note Finish the request by setting status and calling done exactly once, failure included
    void (*submit)(const blk_device_t* restrict device, blk_request_t* restrict req);
    void (*release)(const blk_device_t* restrict device);
};

struct blk_device_private_s {
    blk_device_t* next;
    blk_name_t* name;
    unsigned name_iterator;
    unsigned refcount;
};

struct blk_device_s {
    blk_device_private_t priv;
    const blk_device_ops_t* ops;
    char name[64];
    size_t sector_count;
    size_t sector_size;
    size_t max_transfer;
    size_t max_segments;
    uintptr_t dma_limit; // Highest physical address a segment may reach
    uintptr_t dma_align; // A mask: segment addresses and lengths have these bits clear
    unsigned partition;
    unsigned flags;
    void* ctxdata;
};

int blk_device_add_root(blk_device_t* restrict device, blk_name_t* restrict name, unsigned iterator, unsigned index);
int blk_device_add_part(blk_device_t* restrict device, const blk_device_t* restrict root, unsigned partition);
int blk_device_add_named(blk_device_t* restrict device);

/// Takes the device out of the registry and drops the reference it held there; holders of
/// other references keep the structure, but their requests fail with -ENODEV from now on
int blk_device_remove(blk_device_t* restrict device);

void blk_device_incref(const blk_device_t* restrict device);
void blk_device_decref(const blk_device_t* restrict device);

const blk_device_t* blk_device_find(const char* restrict name) __nodiscard;

/// @pre The caller holds blk_event_lock, which keeps the device list still
void blk_device_for_each(void (*callback)(const blk_device_t* restrict device, void* restrict arg), void* restrict arg);

/// Starts the request and returns; done reports the outcome, even for a request
/// refused right away as -ENODEV or -EINVAL. Segments are physical addresses
/// within dma_limit and dma_align, the length a multiple of the sector size
void blk_submit(const blk_device_t* restrict device, blk_request_t* restrict req);

/// blk_submit, then waits for done; replaces whatever done and ctxdata the request had
/// @pre Interrupts are enabled, unless the driver finishes the request before blk_submit returns
/// @return The request's status
int blk_submit_wait(const blk_device_t* restrict device, blk_request_t* restrict req);

/// Reads synchronously, in as many requests within the device's limits as the transfer needs
/// @pre Interrupts are enabled, unless the driver finishes requests before blk_submit returns
/// @param count Sectors to transfer
/// @param buffer Any kernel memory; pages the device can't reach by DMA go through a bounce buffer
/// @retval -ENOMEM No memory for the bounce buffer
int blk_read(const blk_device_t* restrict device, uintptr_t lba, size_t count, void* restrict buffer);

/// Writes synchronously, in as many requests within the device's limits as the transfer needs
/// @pre Interrupts are enabled, unless the driver finishes requests before blk_submit returns
/// @param count Sectors to transfer
/// @param buffer Any kernel memory; pages the device can't reach by DMA go through a bounce buffer
/// @retval -ENOMEM No memory for the bounce buffer
int blk_write(const blk_device_t* restrict device, uintptr_t lba, size_t count, const void* restrict buffer);

int blk_flush(const blk_device_t* restrict device);
int blk_read_sector_prefix(const blk_device_t* restrict device, uintptr_t lba, void* restrict buffer, size_t size);
