#include <core/errno.h>
#include <dev/blk_device.h>
#include <dev/blk_event.h>
#include <dev/blk_ptable.h>
#include <init/initcall.h>
#include <mm/malloc.h>

struct blk_part_device {
    struct blk_device base;
    struct blk_part_device* next;
    const struct blk_device* root;
    uintptr_t lba_start;
    size_t lba_count;
};

static struct blk_ptable* ptable_head = nullptr;

static int part_op_read(const struct blk_device* restrict device, uintptr_t lba, size_t count, void* restrict buffer)
{
    const struct blk_part_device* part = (const struct blk_part_device*)(device);

    if(lba + count > part->lba_count)
        return EINVAL;
    return blk_device_read(part->root, part->lba_start + lba, count, buffer);
}

static int part_op_write(const struct blk_device* restrict device, uintptr_t lba, size_t count, const void* restrict buffer)
{
    const struct blk_part_device* part = (const struct blk_part_device*)(device);

    if(lba + count > part->lba_count)
        return EINVAL;
    return blk_device_write(part->root, part->lba_start + lba, count, buffer);
}

static int part_op_flush(const struct blk_device* restrict device)
{
    const struct blk_part_device* part = (const struct blk_part_device*)(device);

    return blk_device_flush(part->root);
}

static void part_op_destroy(const struct blk_device* restrict device)
{
    // empty
}

static const struct blk_device_ops part_ops = {
    .read = &part_op_read,
    .write = &part_op_write,
    .flush = &part_op_flush,
    .destroy = &part_op_destroy,
};

static void probe_device(const struct blk_device* restrict device)
{
    struct blk_ptable* it;
    struct blk_ptable_info info;
    struct blk_part_device* part;
    unsigned index;
    void* ptdata;

    for(it = ptable_head; it; it = it->priv.next) {
        if(it->ops->detect(device, &ptdata)) {
            continue;
        }

        index = 0;

        for(;;) {
            if(it->ops->readpt(device, ptdata, index, &info)) {
                break;
            }

            part = kmalloc(sizeof(struct blk_part_device));

            if(part == nullptr) {
                break;
            }

            part->root = device;
            part->lba_start = info.lba_start;
            part->lba_count = info.lba_count;
            part->next = nullptr;

            part->base.ops = &part_ops;
            part->base.sector_count = info.lba_count;
            part->base.sector_size = device->sector_size;
            part->base.flags = BLK_FLAG_PARTITION | (device->flags & BLK_FLAG_PRESENT);
            part->base.ctxdata = nullptr;

            if(blk_device_register_part(&part->base, device->priv.name, device->priv.name_iterator, info.part_index)) {
                kfree(part);
            }
        }

        it->ops->release(ptdata);
        return;
    }
}

static void handle_blk_event(unsigned event, const struct blk_device* restrict device, void* restrict ctxdata)
{
    if(event == BLK_EVENT_CONNECTED && 0 == (device->flags & BLK_FLAG_PARTITION)) {
        probe_device(device);
    }
}

int blk_ptable_register(struct blk_ptable* restrict ptable)
{
    struct blk_ptable* it;

    for(it = ptable_head; it; it = it->priv.next) {
        if(it == ptable) {
            return EEXIST;
        }
    }

    ptable->priv.next = ptable_head;
    ptable_head = ptable;

    return 0;
}

int blk_ptable_unregister(struct blk_ptable* restrict ptable)
{
    struct blk_ptable* it;
    struct blk_ptable* prev = nullptr;

    if(ptable_head == ptable) {
        ptable_head = ptable->priv.next;
        return 0;
    }

    for(it = ptable_head; it; it = it->priv.next) {
        if(it == ptable) {
            prev->priv.next = it->priv.next;
            return 0;
        }

        prev = it;
    }

    return ENODEV;
}

define_initcall(blk_ptable, INITCALL_STAGE_BSP_DEV, INITCALL_ORDER_FIRST)
{
    blk_event_register_handler(&handle_blk_event, nullptr);
}