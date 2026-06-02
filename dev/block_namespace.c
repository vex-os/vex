#include <core/assert.h>
#include <dev/block_namespace.h>
#include <init/initcall.h>
#include <lib/sprintf.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <mm/malloc.h>

static struct block_namespace* block_namespace_head = nullptr;

struct block_namespace* block_namespace_ata;
struct block_namespace* block_namespace_atapi;
struct block_namespace* block_namespace_nvme;
struct block_namespace* block_namespace_scsi;
struct block_namespace* block_namespace_virtio;

struct block_namespace* block_namespace_init(const char* restrict prefix)
{
    struct block_namespace* it;

    for(it = block_namespace_head; it; it = it->priv.next) {
        if(strcmp(it->prefix, prefix) == 0) {
            return it;
        }
    }

    it = kmalloc(sizeof(struct block_namespace));

    if(it == nullptr) {
        return nullptr;
    }

    kstrncpy(it->prefix, prefix, sizeof(it->prefix));

    it->priv.next = block_namespace_head;
    it->priv.next_device_id = 0;
    block_namespace_head = it;

    return it;
}

unsigned block_namespace_next_id(struct block_namespace* restrict namesp)
{
    unsigned result = namesp->priv.next_device_id;
    namesp->priv.next_device_id += 1;
    return result;
}

unsigned block_namespace_size(const struct block_namespace* restrict namesp)
{
    return namesp->priv.next_device_id;
}

define_initcall(block_namespace_predef, INITCALL_STAGE_BSP_BUS, INITCALL_ORDER_LATE)
{
    block_namespace_ata = block_namespace_init("ata");
    block_namespace_atapi = block_namespace_init("atapi");
    block_namespace_nvme = block_namespace_init("nvme");
    block_namespace_scsi = block_namespace_init("scsi");
    block_namespace_virtio = block_namespace_init("virtio");

    strict_kassert_text(block_namespace_ata, "out of memory");
    strict_kassert_text(block_namespace_atapi, "out of memory");
    strict_kassert_text(block_namespace_nvme, "out of memory");
    strict_kassert_text(block_namespace_scsi, "out of memory");
    strict_kassert_text(block_namespace_virtio, "out of memory");
}
