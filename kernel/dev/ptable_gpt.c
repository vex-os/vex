#include <vex/blk_device.h>
#include <vex/blk_ptable.h>
#include <vex/compiler.h>
#include <vex/crc32_iso3309.h>
#include <vex/endian.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/string.h>

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

// Upper bound on the partition entry array;
// the specification asks for at least 16 KiB
// and anything far beyond that is corrupted or hostile
#define GPT_ENTRIES_MAX_SIZE (1024 * 1024)

static int gpt_check_header(const struct blk_device* restrict device, uintptr_t header_lba, struct gpt_ptdata** restrict ptdata)
{
    uint8_t* sector = kmalloc(device->sector_size);

    if(sector == nullptr) {
        return -ENOMEM;
    }

    int retval = blk_read(device, header_lba, 1, sector);

    if(retval < 0) {
        kfree(sector);
        return retval;
    }

    struct gpt_header header;
    memcpy(&header, sector, sizeof(struct gpt_header));

    uint32_t header_size = le32toh(header.header_size);

    if(memcmp(header.signature, "EFI PART", 8) || header_size < sizeof(struct gpt_header) || header_size > device->sector_size) {
        kfree(sector);
        return -EINVAL;
    }

    // The checksum covers header_size bytes with its own field zeroed,
    // which may extend past the fields this structure knows about
    uint32_t checksum = le32toh(header.header_crc32);
    memset(sector + offsetof(struct gpt_header, header_crc32), 0, sizeof(uint32_t));
    uint32_t computed = crc32_iso3309_get(sector, header_size);

    kfree(sector);

    if(checksum != computed) {
        return -EINVAL;
    }

    uint32_t entry_count = le32toh(header.entry_count);
    uint32_t entry_size = le32toh(header.entry_size);
    uint64_t entry_start_lba = le64toh(header.entry_start_lba);

    if(entry_size < sizeof(struct gpt_entry) || entry_size % 8 != 0) {
        return -EINVAL;
    }

    if(entry_count == 0 || entry_count > GPT_ENTRIES_MAX_SIZE / entry_size) {
        return -EINVAL;
    }

    size_t entries_size = (size_t)entry_count * entry_size;
    size_t entries_lbas = (entries_size + device->sector_size - 1) / device->sector_size;

    if(entry_start_lba >= device->sector_count || entries_lbas > device->sector_count - entry_start_lba) {
        return -EINVAL;
    }

    // Whole sectors are read, so the buffer is rounded up to them
    struct gpt_ptdata* gpt = kmalloc(entries_lbas * device->sector_size + sizeof(struct gpt_ptdata));

    if(gpt == nullptr) {
        return -ENOMEM;
    }

    retval = blk_read(device, entry_start_lba, entries_lbas, gpt->cached_entries);

    if(retval < 0) {
        kfree(gpt);
        return retval;
    }

    if(le32toh(header.entry_crc32) != crc32_iso3309_get(gpt->cached_entries, entries_size)) {
        kfree(gpt);
        return -EINVAL;
    }

    memcpy(&gpt->header, &header, sizeof(struct gpt_header));

    *ptdata = gpt;

    return 0;
}

static int gpt_detect(const struct blk_device* restrict device, void** restrict ptdata)
{
    struct pmbr_sector sector;
    int retval = blk_read_sector_prefix(device, 0, &sector, sizeof(sector));

    if(retval < 0) {
        return retval;
    }

    bool is_valid = true;
    is_valid = is_valid && sector.signature[0] == PMBR_SIGNATURE_0;
    is_valid = is_valid && sector.signature[1] == PMBR_SIGNATURE_1;
    is_valid = is_valid && sector.entries[0].type == 0xEE;
    is_valid = is_valid && sector.entries[1].type == 0x00;
    is_valid = is_valid && sector.entries[2].type == 0x00;
    is_valid = is_valid && sector.entries[3].type == 0x00;

    if(!is_valid) {
        return -EINVAL;
    }

    struct gpt_ptdata* gpt;
    retval = gpt_check_header(device, le32toh(sector.entries[0].lba_start), &gpt);

    if(retval < 0) {
        retval = gpt_check_header(device, device->sector_count - 1, &gpt);
    }

    if(retval < 0) {
        return retval;
    }

    *ptdata = gpt;

    return 0;
}

static int gpt_readpt(const struct blk_device* restrict device, void* restrict ptdata, unsigned index, struct blk_ptable_info* restrict out)
{
    static const uint8_t null_guid[16] = { 0 }; // FIXME: a GUID library would be nice

    const struct gpt_ptdata* gpt = ptdata;
    uint32_t entry_count = le32toh(gpt->header.entry_count);
    uint32_t entry_size = le32toh(gpt->header.entry_size);

    unsigned found = 0;

    for(unsigned slot = 0; slot < entry_count; slot += 1) {
        const struct gpt_entry* entry = (const struct gpt_entry*)(gpt->cached_entries + entry_size * slot);

        if(0 == memcmp(entry->type_guid, null_guid, 16)) {
            continue;
        }

        uint64_t lba_start = le64toh(entry->lba_start);
        uint64_t lba_end = le64toh(entry->lba_end);

        if(lba_end < lba_start || lba_end >= device->sector_count) {
            continue;
        }

        if(found == index) {
            const uint16_t* name = (const uint16_t*)((const uint8_t*)entry + sizeof(struct gpt_entry));
            size_t name_len = (entry_size - sizeof(struct gpt_entry)) / sizeof(uint16_t);

            unsigned i;

            for(i = 0; i < name_len && i < 63 && name[i]; ++i) {
                out->part_label[i] = le16toh(name[i]) & 0x7F;
            }

            out->part_label[i] = 0x00;
            out->part_index = slot;
            out->lba_start = lba_start;
            out->lba_count = lba_end - lba_start + 1;
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

static void init_ptable_gpt(void)
{
    add_blk_ptable(&gpt_ptable);
}

initcall_bsp_dev(init_ptable_gpt, INIT_FIRST);
