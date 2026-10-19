#pragma once

#include <vex/compiler.h>
#include <vex/mutex.h>
#include <vex/path.h>
#include <vex/types.h>

struct blk_device;
struct vnode;

struct echfs_identity {
    uint8_t reserved_1[4];
    uint8_t signature[8];
    uint64_t total_blocks;
    uint64_t maindir_size;
    uint64_t block_size;
    uint32_t reserved_2;
    uint8_t part_uuid[16];
    uint8_t reserved_3[454];
    uint8_t mbr_signature[2];
} __packed;

#ifndef __INTELLISENSE__
static_assert(512 == sizeof(struct echfs_identity));
#endif

#define ECHFS_TABLE_FREE     UINT64_C(0x0000000000000000)
#define ECHFS_TABLE_RESERVED UINT64_C(0xFFFFFFFFFFFFFFF0)
#define ECHFS_TABLE_EOC      UINT64_C(0xFFFFFFFFFFFFFFFF)

#define ECHFS_DIR_ID_END     UINT64_C(0x00000000000000000)
#define ECHFS_DIR_ID_DELETED UINT64_C(0xFFFFFFFFFFFFFFFE)
#define ECHFS_DIR_ID_ROOT    UINT64_C(0xFFFFFFFFFFFFFFFF)

struct echfs_dirent {
    uint64_t directory_id;
    uint8_t object_type;
    uint8_t name[201];
    uint64_t access_time;
    uint64_t modify_time;
    uint16_t permissions;
    uint16_t owner_uid;
    uint16_t owner_gid;
    uint64_t change_time; // unix ctime
    uint64_t start_block; // or directory_id for directories
    uint64_t file_size;
} __packed;

#ifndef __INTELLISENSE__
static_assert(256 == sizeof(struct echfs_dirent));
#endif

struct echfs_vfs_context {
    const struct blk_device* device;

    bool param_readonly;

    uint64_t block_size;
    uint64_t sectors_per_block;
    uint64_t total_blocks;
    uint64_t fat_start;
    uint64_t fat_size;
    uint64_t dir_start;
    uint64_t dir_size;
    uint64_t data_start;

    uint64_t dir_entries;
    uint64_t next_free_hint;

    struct vnode* devnode;
    struct mutex lock; // Serializes changes to the table and the directory, and the file cache calls for files
};

struct echfs_vnode_context {
    struct echfs_vfs_context* fs_ctx;
    uint64_t dir_entry;   // Index into dir_table; UINT64_MAX for the root
    uint64_t start_block; // First data block (files) or directory ID (dirs)
    uint64_t size;
    uint64_t cached_block;
    uint64_t cached_block_idx;
    bool unlinked; // The entry at dir_entry is gone, blocks are freed on release
};

extern const struct vnode_ops echfs_vnode_ops;

uint64_t echfs_block_entry(const struct echfs_vfs_context* restrict context, uintptr_t block) __nodiscard;
uintptr_t echfs_block_alloc(struct echfs_vfs_context* restrict context) __nodiscard;
void echfs_block_free(struct echfs_vfs_context* restrict context, uintptr_t block);
void echfs_block_link(struct echfs_vfs_context* restrict context, uintptr_t block, uintptr_t next);
uintptr_t echfs_block_next(const struct echfs_vfs_context* restrict context, uintptr_t block) __nodiscard;

int echfs_dirent_find(const struct echfs_vfs_context* restrict context, uint64_t parent_id, const char* restrict name,
    uint64_t* restrict out_index);
int echfs_dirent_read(const struct echfs_vfs_context* restrict context, uint64_t index, struct echfs_dirent* restrict out);
int echfs_dirent_write(struct echfs_vfs_context* restrict context, uint64_t index, const struct echfs_dirent* restrict entry);
int echfs_dirent_alloc(struct echfs_vfs_context* restrict context, uint64_t* restrict out_index) __nodiscard;

static __always_inline uintptr_t echfs_block_lba(const struct echfs_vfs_context* restrict context, uintptr_t block)
{
    return block * context->sectors_per_block;
}
