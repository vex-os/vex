#include <core/assert.h>
#include <dev/blk_name.h>
#include <init/initcall.h>
#include <lib/sprintf.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <mm/malloc.h>

static struct blk_name* blk_name_head = nullptr;

struct blk_name* blk_name_ata;
struct blk_name* blk_name_atapi;
struct blk_name* blk_name_nvme;
struct blk_name* blk_name_virtio;

struct blk_name* blk_name_query(const char* restrict prefix)
{
    struct blk_name* name;

    for(name = blk_name_head; name; name = name->priv.next) {
        if(strcmp(name->device_prefix, prefix) == 0) {
            return name;
        }
    }

    name = kmalloc(sizeof(struct blk_name));

    if(name == nullptr) {
        return nullptr;
    }

    kstrncpy(name->device_prefix, prefix, sizeof(name->device_prefix));

    name->priv.next = blk_name_head;
    name->priv.next_iterator = 0;
    blk_name_head = name;

    return name;
}

unsigned blk_name_increment(struct blk_name* restrict name)
{
    unsigned result = name->priv.next_iterator;
    name->priv.next_iterator += 1;
    return result;
}

unsigned blk_name_count(const struct blk_name* restrict name)
{
    return name->priv.next_iterator;
}

int blk_name_format(char* restrict buffer, size_t sz, const struct blk_name* restrict name, unsigned iterator, unsigned index)
{
    return snprintf(buffer, sz, "%s%un%u", name->device_prefix, iterator, index);
}

int blk_name_format_partition(char* restrict buffer, size_t sz, const struct blk_name* restrict name, unsigned iterator, unsigned index,
    unsigned partition)
{
    return snprintf(buffer, sz, "%s%un%up%u", name->device_prefix, iterator, index, partition);
}

define_initcall(blk_name_predefined, INITCALL_STAGE_BSP_BUS, INITCALL_ORDER_LATE)
{
    blk_name_ata = blk_name_query(BLK_NAME_PREFIX_ATA);
    blk_name_atapi = blk_name_query(BLK_NAME_PREFIX_ATAPI);
    blk_name_nvme = blk_name_query(BLK_NAME_PREFIX_NVME);
    blk_name_virtio = blk_name_query(BLK_NAME_PREFIX_VIRTIO);

    strict_kassert_text(blk_name_ata, "out of memory");
    strict_kassert_text(blk_name_atapi, "out of memory");
    strict_kassert_text(blk_name_nvme, "out of memory");
    strict_kassert_text(blk_name_virtio, "out of memory");
}
