#include <core/cdefs.h>
#include <core/endian.h>
#include <core/errno.h>
#include <dev/blk_device.h>
#include <dev/blk_ptable.h>
#include <init/initcall.h>
#include <lib/string.h>
#include <mm/malloc.h>

#define MBR_SIGNATURE_0 UINT8_C(0x55)
#define MBR_SIGNATURE_1 UINT8_C(0xAA)
#define MBR_ENTRY_COUNT 4

struct mbr_entry {
    uint8_t status;
    uint8_t chs_first[3];
    uint8_t type;
    uint8_t chs_last[3];
    uint32_t lba_start;
    uint32_t lba_count;
} __packed;

struct mbr_sector {
    uint8_t bootstrap[440];
    uint8_t disk_signature[4];
    uint8_t reserved[2];
    struct mbr_entry entries[MBR_ENTRY_COUNT];
    uint8_t signature[2];
} __packed;

#ifndef __INTELLISENSE__
static_assert(16 == sizeof(struct mbr_entry));
static_assert(512 == sizeof(struct mbr_sector)); // shutcho damn mouth it's not 516
#endif

static int mbr_detect(const struct blk_device* restrict device, void** restrict ptdata)
{
    bool is_protective;
    bool is_valid;
    int retval;
    struct mbr_sector sector;
    struct mbr_sector* sector_copy;

    retval = blk_device_read(device, 0, 1, &sector);

    if(retval < 0) {
        return retval;
    }

    is_valid = true;
    is_valid = is_valid && sector.signature[0] == MBR_SIGNATURE_0;
    is_valid = is_valid && sector.signature[1] == MBR_SIGNATURE_1;

    if(is_valid) {
        is_protective = true;
        is_protective = is_protective && sector.entries[0].type == 0xEE;
        is_protective = is_protective && sector.entries[1].type == 0x00;
        is_protective = is_protective && sector.entries[2].type == 0x00;
        is_protective = is_protective && sector.entries[3].type == 0x00;

        if(is_protective) {
            return -EINVAL;
        }

        sector_copy = kmalloc(sizeof(struct mbr_sector));

        if(sector_copy == nullptr) {
            return -ENOMEM;
        }

        memcpy(sector_copy, &sector, sizeof(struct mbr_sector));

        ptdata[0] = sector_copy;

        return 0;
    }

    return -EINVAL;
}

static int mbr_readpt(const struct blk_device* restrict device, void* restrict ptdata, unsigned index, struct blk_ptable_info* restrict out)
{
    const struct mbr_sector* mbr = ptdata;

    const struct mbr_entry* entry;
    unsigned found;
    unsigned slot;

    found = 0;

    for(slot = 0; slot < MBR_ENTRY_COUNT; slot += 1) {
        entry = &mbr->entries[slot];

        if(entry->type == 0x00 || entry->lba_count == 0) {
            continue;
        }

        if(found == index) {
            out->part_label[0] = 0x00;
            out->part_index = slot;
            out->lba_start = LE32_to_host(entry->lba_start);
            out->lba_count = LE32_to_host(entry->lba_count);
            out->type = entry->type;
            return 0;
        }

        found += 1;
    }

    return -ENOENT;
}

static void mbr_release(void* restrict ptdata)
{
    kfree(ptdata);
}

static const struct blk_ptable_ops mbr_ops = {
    .detect = &mbr_detect,
    .readpt = &mbr_readpt,
    .release = &mbr_release,
};

static struct blk_ptable mbr_ptable = {
    .ops = &mbr_ops,
    .identifier = "mbr",
};

define_initcall(ptable_mbr, INITCALL_STAGE_BSP_DEV, INITCALL_ORDER_FIRST)
{
    blk_ptable_register(&mbr_ptable);
}
