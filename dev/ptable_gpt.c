#include <core/cdefs.h>
#include <core/endian.h>
#include <core/errno.h>
#include <dev/blk_device.h>
#include <dev/blk_ptable.h>
#include <hash/crc32_iso3309.h>
#include <init/initcall.h>
#include <lib/string.h>
#include <mm/malloc.h>

#define PMBR_SIGNATURE_0 UINT8_C(0x55)
#define PMBR_SIGNATURE_1 UINT8_C(0xAA)
#define PMBR_ENTRY_COUNT 4

struct pmbr_entry {
    uint8_t status;
    uint8_t chs_first[3];
    uint8_t type;
    uint8_t chs_last[3];
    uint32_t lba_start;
    uint32_t lba_count;
} __packed;

struct pmbr_sector {
    uint8_t bootstrap[440];
    uint8_t disk_signature[4];
    uint8_t reserved[2];
    struct pmbr_entry entries[PMBR_ENTRY_COUNT];
    uint8_t signature[2];
} __packed;

#ifndef __INTELLISENSE__
static_assert(16 == sizeof(struct pmbr_entry));
static_assert(512 == sizeof(struct pmbr_sector)); // shut your damn mouth it's not 516
#endif

struct gpt_header {
    uint8_t signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t alternate_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t disk_guid[16];
    uint64_t entry_start_lba;
    uint32_t entry_count;
    uint32_t entry_size;
    uint32_t entry_crc32;
} __packed;

struct gpt_entry {
    uint8_t type_guid[16];
    uint8_t unique_guid[16];
    uint64_t lba_start;
    uint64_t lba_end;
    uint64_t attributes;
    uint16_t part_name[0];
} __packed;

#ifndef __INTELLISENSE__
static_assert(92 == sizeof(struct gpt_header)); // shut your damn mouth it's not 96
static_assert(56 == sizeof(struct gpt_entry));
#endif

struct gpt_ptdata {
    struct gpt_header header;
    uint8_t cached_entries[0];
};

static int gpt_check_header(const struct blk_device* restrict device, uintptr_t header_lba, struct gpt_ptdata** restrict ptdata)
{
    struct gpt_header* sector;
    struct gpt_header header;
    struct gpt_ptdata* gpt;
    size_t entries_size;
    size_t entries_lbas;
    uint32_t checksum;
    int retval;

    sector = kmalloc(device->sector_size);

    if(sector == nullptr) {
        return -ENOMEM;
    }

    retval = blk_device_read(device, header_lba, 1, sector);

    if(retval < 0) {
        kfree(sector);
        return retval;
    }

    memcpy(&header, sector, sizeof(struct gpt_header));
    kfree(sector);

    checksum = LE32_to_host(header.header_crc32);
    header.header_crc32 = UINT32_C(0);

    if(memcmp(header.signature, "EFI PART", 8)) {
        return -EINVAL;
    }

    if(checksum == crc32_iso3309_get(&header, LE32_to_host(header.header_size))) {
        entries_size = (size_t)(LE32_to_host(header.entry_count) * LE32_to_host(header.entry_size));
        gpt = kmalloc(entries_size + sizeof(struct gpt_ptdata));

        if(gpt == nullptr) {
            return -ENOMEM;
        }

        entries_lbas = (entries_size + device->sector_size - 1) / device->sector_size;
        retval = blk_device_read(device, LE64_to_host(header.entry_start_lba), entries_lbas, gpt->cached_entries);

        if(retval < 0) {
            kfree(gpt);
            return retval;
        }

        if(LE32_to_host(header.entry_crc32) != crc32_iso3309_get(gpt->cached_entries, entries_size)) {
            kfree(gpt);
            return -EINVAL;
        }

        memcpy(&gpt->header, &header, sizeof(struct gpt_header));

        ptdata[0] = gpt;

        return 0;
    }

    return -EINVAL;
}

static int gpt_detect(const struct blk_device* restrict device, void** restrict ptdata)
{
    struct pmbr_sector sector;
    struct gpt_ptdata* gpt;
    bool is_valid;
    int retval;

    retval = blk_device_read(device, 0, 1, &sector);

    if(retval < 0) {
        return retval;
    }

    is_valid = true;
    is_valid = is_valid && sector.signature[0] == PMBR_SIGNATURE_0;
    is_valid = is_valid && sector.signature[1] == PMBR_SIGNATURE_1;
    is_valid = is_valid && sector.entries[0].type == 0xEE;
    is_valid = is_valid && sector.entries[1].type == 0x00;
    is_valid = is_valid && sector.entries[2].type == 0x00;
    is_valid = is_valid && sector.entries[3].type == 0x00;

    if(!is_valid) {
        return -EINVAL;
    }

    retval = gpt_check_header(device, LE32_to_host(sector.entries[0].lba_start), &gpt);

    if(retval < 0) {
        retval = gpt_check_header(device, device->sector_count - 1, &gpt);
    }

    if(retval < 0) {
        return retval;
    }

    ptdata[0] = gpt;

    return 0;
}

static int gpt_readpt(const struct blk_device* restrict device, void* restrict ptdata, unsigned index, struct blk_ptable_info* restrict out)
{
    static const uint8_t null_guid[16] = { 0 }; // FIXME: a GUID library would be nice

    const struct gpt_ptdata* gpt = ptdata;
    const struct gpt_entry* entry;
    uint32_t entry_count = LE32_to_host(gpt->header.entry_count);
    uint32_t entry_size = LE32_to_host(gpt->header.entry_size);
    unsigned found, slot, i;
    const uint16_t* name;
    size_t name_len;

    found = 0;

    for(slot = 0; slot < entry_count; slot += 1) {
        entry = (const struct gpt_entry*)(gpt->cached_entries + entry_size * (uint32_t)(slot));

        if(0 == memcmp(entry->type_guid, null_guid, 16)) {
            continue;
        }

        if(found == index) {
            name = (const uint16_t*)((const uint8_t*)(entry) + sizeof(struct gpt_entry));
            name_len = (entry_size - sizeof(struct gpt_entry)) / sizeof(uint16_t);

            for(i = 0; i < name_len && i < 63 && name[i]; i += 1) {
                out->part_label[i] = (char)(LE16_to_host(name[i]) & 0x7F);
            }

            out->part_label[i] = 0x00;
            out->part_index = slot;
            out->lba_start = LE64_to_host(entry->lba_start);
            out->lba_count = LE64_to_host(entry->lba_end) - LE64_to_host(entry->lba_start) + 1;
            out->type = 0; // FIXME: figure out a way to shove a GUID into an uint64_t

            return 0;
        }

        found += 1;
    }

    return -ENOENT;
}

static void gpt_release(void* ptdata)
{
    kfree(ptdata);
}

static const struct blk_ptable_ops gpt_ptable_ops = {
    .detect = &gpt_detect,
    .readpt = &gpt_readpt,
    .release = &gpt_release,
};

static struct blk_ptable gpt_ptable = {
    .ops = &gpt_ptable_ops,
    .identifier = "gpt",
};

define_initcall(gpt_ptable_init, INITCALL_STAGE_BSP_DEV, INITCALL_ORDER_FIRST)
{
    blk_ptable_register(&gpt_ptable);
}
