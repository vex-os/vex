#ifndef DDF5F45D_960D_439C_8511_CE0CBA3BAEAF
#define DDF5F45D_960D_439C_8511_CE0CBA3BAEAF
#pragma once

#include <core/cdefs.h>
#include <stddef.h>

struct block_namespace;
struct block_namespace_private {
    struct block_namespace* next;
    unsigned next_device_id;
};

struct block_namespace {
    struct block_namespace_private priv;
    char prefix[32];
};

extern struct block_namespace* block_namespace_ata;
extern struct block_namespace* block_namespace_atapi;
extern struct block_namespace* block_namespace_nvme;
extern struct block_namespace* block_namespace_scsi;
extern struct block_namespace* block_namespace_virtio;

extern struct block_namespace* block_namespace_query(const char* restrict prefix) __nodiscard;
extern unsigned block_namespace_next_id(struct block_namespace* restrict namesp) __nodiscard;
extern unsigned block_namespace_size(const struct block_namespace* restrict namesp) __nodiscard;

#endif /* DDF5F45D_960D_439C_8511_CE0CBA3BAEAF */
