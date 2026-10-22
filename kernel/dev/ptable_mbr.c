#include <vex/block_interface.h>
#include <vex/block_ptable.h>
#include <vex/compiler.h>
#include <vex/endian.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/string.h>

#define MBR_SIGNATURE_0 UINT8_C(0x55)
#define MBR_SIGNATURE_1 UINT8_C(0xAA)
#define MBR_ENTRY_COUNT 4

typedef struct mbr_entry_s mbr_entry_t;
typedef struct mbr_sector_s mbr_sector_t;

struct mbr_entry_s {
    uint8_t status;
    uint8_t chs_first[3];
    uint8_t type;
    uint8_t chs_last[3];
    uint32_t lba_start;
    uint32_t lba_count;
} __packed;

struct mbr_sector_s {
    uint8_t bootstrap[440];
    uint8_t disk_signature[4];
    uint8_t reserved[2];
    mbr_entry_t entries[MBR_ENTRY_COUNT];
    uint8_t signature[2];
} __packed;

#ifndef __INTELLISENSE__
static_assert(16 == sizeof(mbr_entry_t));
static_assert(512 == sizeof(mbr_sector_t));
#endif

static int mbr_detect(const block_interface_t* restrict interface, void** restrict ptdata)
{
    mbr_sector_t sector;
    int retval = block_read_sector_prefix(interface, 0, &sector, sizeof(sector));

    if(retval < 0) {
        return retval;
    }

    bool is_valid = true;
    is_valid = is_valid && sector.signature[0] == MBR_SIGNATURE_0;
    is_valid = is_valid && sector.signature[1] == MBR_SIGNATURE_1;

    if(is_valid) {
        bool is_protective = true;
        is_protective = is_protective && sector.entries[0].type == 0xEE;
        is_protective = is_protective && sector.entries[1].type == 0x00;
        is_protective = is_protective && sector.entries[2].type == 0x00;
        is_protective = is_protective && sector.entries[3].type == 0x00;

        if(is_protective) {
            return -EINVAL;
        }

        mbr_sector_t* sector_copy = kmalloc(sizeof(mbr_sector_t));

        if(sector_copy == nullptr) {
            return -ENOMEM;
        }

        memcpy(sector_copy, &sector, sizeof(mbr_sector_t));

        *ptdata = sector_copy;

        return 0;
    }

    return -EINVAL;
}

static int mbr_read_table(const block_interface_t* restrict interface, void* restrict ptdata, unsigned index,
    block_partition_table_info_t* restrict out)
{
    const mbr_sector_t* mbr = ptdata;

    unsigned found = 0;

    for(unsigned slot = 0; slot < MBR_ENTRY_COUNT; slot += 1) {
        const mbr_entry_t* entry = &mbr->entries[slot];

        if(entry->type == 0x00 || entry->lba_count == 0) {
            continue;
        }

        uint64_t lba_start = le32toh(entry->lba_start);
        uint64_t lba_count = le32toh(entry->lba_count);

        if(lba_start >= interface->sector_count || lba_count > interface->sector_count - lba_start) {
            continue;
        }

        if(found == index) {
            out->part_label[0] = 0x00;
            out->part_index = slot;
            out->lba_start = lba_start;
            out->lba_count = lba_count;
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

static const block_partition_table_ops_t mbr_ops = {
    .detect = &mbr_detect,
    .read_table = &mbr_read_table,
    .release = &mbr_release,
};

static block_partition_table_t mbr_ptable = {
    .ops = &mbr_ops,
    .name = "mbr",
};

static void init_ptable_mbr(void)
{
    block_partition_table_add(&mbr_ptable);
}

initcall_bsp_dev(init_ptable_mbr, INIT_FIRST);
