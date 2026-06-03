#include <core/cdefs.h>
#include <core/errno.h>
#include <dev/blk_device.h>
#include <dev/blk_ptable.h>
#include <init/initcall.h>
#include <mm/malloc.h>
#include <stdint.h>

#define MBR_SIGNATURE    0xAA55U
#define MBR_ENTRY_COUNT  4

struct mbr_entry {
    uint8_t  status;
    uint8_t  chs_first[3];
    uint8_t  type;
    uint8_t  chs_last[3];
    uint32_t lba_start;
    uint32_t lba_count;
} __packed;

struct mbr_sector {
    uint8_t          bootstrap[446];
    struct mbr_entry entries[MBR_ENTRY_COUNT];
    uint16_t         signature;
} __packed;

static int mbr_detect(const struct blk_device* restrict device, void** restrict ptdata)
{
    struct mbr_sector* mbr;
    int result;

    mbr = kmalloc(sizeof(struct mbr_sector));
    if(mbr == nullptr)
        return ENOMEM;

    result = blk_device_read(device, 0, 1, mbr);
    if(result != 0) {
        kfree(mbr);
        return result;
    }

    if(mbr->signature != MBR_SIGNATURE) {
        kfree(mbr);
        return EINVAL;
    }

    if(mbr->entries[0].type == 0xEE &&
       mbr->entries[1].type == 0x00 &&
       mbr->entries[2].type == 0x00 &&
       mbr->entries[3].type == 0x00) {
        kfree(mbr);
        return EINVAL;
    }

    *ptdata = mbr;
    return 0;
}

static int mbr_readpt(const struct blk_device* restrict device, void* restrict ptdata, unsigned index, struct blk_ptable_info* restrict out)
{
    const struct mbr_sector* mbr = ptdata;
    const struct mbr_entry* entry;
    unsigned slot;
    unsigned found;

    (void)device;

    found = 0;

    for(slot = 0; slot < MBR_ENTRY_COUNT; slot++) {
        entry = &mbr->entries[slot];

        if(entry->type == 0 || entry->lba_count == 0)
            continue;

        if(found == index) {
            out->part_label[0] = '\0';
            out->part_index    = slot;
            out->lba_start     = entry->lba_start;
            out->lba_count     = entry->lba_count;
            out->type          = entry->type;
            return 0;
        }

        found++;
    }

    return ENOENT;
}

static void mbr_release(void* restrict ptdata)
{
    kfree(ptdata);
}

static const struct blk_ptable_ops mbr_ops = {
    .detect  = mbr_detect,
    .readpt  = mbr_readpt,
    .release = mbr_release,
};

static struct blk_ptable mbr_ptable = {
    .ops        = &mbr_ops,
    .identifier = "mbr",
};

define_initcall(ptable_mbr, INITCALL_STAGE_BSP_DEV, INITCALL_ORDER_FIRST)
{
    blk_ptable_register(&mbr_ptable);
}
