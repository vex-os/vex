#pragma once

#include <vex/compiler.h>
#include <vex/errno.h>
#include <vex/types.h>

struct blk_name;

struct blk_segment {
    uintptr_t phys;
    size_t length;
};

enum {
    BLK_REQ_READ,
    BLK_REQ_WRITE,
    BLK_REQ_FLUSH,
};

struct blk_request;
typedef void (*blk_request_done_type)(struct blk_request* restrict req);

struct blk_request {
    unsigned op;
    uintptr_t lba;
    size_t length;
    const struct blk_segment* segments;
    size_t num_segments;
    int status;
    blk_request_done_type done;
    void* ctxdata;
};

struct blk_device;
struct blk_device_ops {
    void (*submit)(const struct blk_device* restrict device, struct blk_request* restrict req);
    void (*release)(const struct blk_device* restrict device);
};

struct blk_device_private {
    struct blk_device* next;
    struct blk_name* name;
    unsigned name_iterator;
    unsigned refcount;
};

#define BLK_FLAG_PRESENT   0x0001 // Media is present and ready
#define BLK_FLAG_PARTITION 0x0002
#define BLK_FLAG_HIDDEN    0x0004 // Device is not added to devfs

struct blk_device {
    struct blk_device_private priv;
    const struct blk_device_ops* ops;
    char identifier[64];
    size_t sector_count;
    size_t sector_size;
    size_t max_transfer;
    size_t max_segments;
    uintptr_t dma_limit;
    uintptr_t dma_align;
    unsigned partition;
    unsigned flags;
    void* ctxdata;
};

int add_blk_device_root(struct blk_device* restrict device, struct blk_name* restrict name, unsigned iterator, unsigned index);
int add_blk_device_part(struct blk_device* restrict device, const struct blk_device* restrict root, unsigned partition);
int add_blk_device_named(struct blk_device* restrict device);
int remove_blk_device(struct blk_device* restrict device);

void blk_device_incref(const struct blk_device* restrict device);
void blk_device_decref(const struct blk_device* restrict device);

const struct blk_device* find_blk_device(const char* restrict name) __nodiscard;
void iterate_blk_devices(void (*callback)(const struct blk_device* restrict device, void* restrict arg), void* restrict arg);

void blk_submit(const struct blk_device* restrict device, struct blk_request* restrict req);
int blk_submit_wait(const struct blk_device* restrict device, struct blk_request* restrict req);

int blk_read(const struct blk_device* restrict device, uintptr_t lba, size_t count, void* restrict buffer);
int blk_write(const struct blk_device* restrict device, uintptr_t lba, size_t count, const void* restrict buffer);
int blk_flush(const struct blk_device* restrict device);
int blk_read_sector_prefix(const struct blk_device* restrict device, uintptr_t lba, void* restrict buffer, size_t size);
