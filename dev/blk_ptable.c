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

struct blk_disk_state {
    struct blk_disk_state* next;
    const struct blk_device* disk;
    struct blk_part_device* parts;
};

static struct blk_ptable* ptable_head = nullptr;
static struct blk_disk_state* disk_state_head = nullptr;

static int part_read(const struct blk_device* restrict device, uintptr_t lba, size_t count, void* restrict buffer)
{
    const struct blk_part_device* part = (const struct blk_part_device*)(device);

    if(lba + count > part->lba_count)
        return -EINVAL;
    return blk_device_read(part->root, part->lba_start + lba, count, buffer);
}

static int part_write(const struct blk_device* restrict device, uintptr_t lba, size_t count, const void* restrict buffer)
{
    const struct blk_part_device* part = (const struct blk_part_device*)(device);

    if(lba + count > part->lba_count)
        return -EINVAL;
    return blk_device_write(part->root, part->lba_start + lba, count, buffer);
}

static int part_flush(const struct blk_device* restrict device)
{
    const struct blk_part_device* part = (const struct blk_part_device*)(device);

    return blk_device_flush(part->root);
}

static void part_destroy(const struct blk_device* restrict device)
{
    kfree((void*)(device));
}

static const struct blk_device_ops part_ops = {
    .read = &part_read,
    .write = &part_write,
    .flush = &part_flush,
    .destroy = &part_destroy,
};

static struct blk_disk_state* disk_state_find(const struct blk_device* restrict disk)
{
    struct blk_disk_state* it;

    for(it = disk_state_head; it; it = it->next) {
        if(it->disk == disk)
            return it;
    }

    return nullptr;
}

static void disk_state_remove_parts(struct blk_disk_state* restrict state)
{
    struct blk_part_device* it;
    struct blk_part_device* next;

    for(it = state->parts; it; it = next) {
        next = it->next;

        blk_event_notify(BLK_EVENT_PARTITION_REMOVED, &it->base);

        blk_device_unregister(&it->base);
    }

    state->parts = nullptr;
}

static void probe_device(const struct blk_device* restrict device)
{
    struct blk_ptable* it;
    struct blk_disk_state* state;
    struct blk_ptable_info info;
    struct blk_part_device* part;
    unsigned index;
    void* ptdata;

    state = disk_state_find(device);

    if(state == nullptr) {
        state = kmalloc(sizeof(struct blk_disk_state));

        if(state == nullptr) {
            return;
        }

        state->disk = device;
        state->parts = nullptr;

        state->next = disk_state_head;
        disk_state_head = state;
    }

    for(it = ptable_head; it; it = it->priv.next) {
        ptdata = nullptr;

        if(it->ops->detect(device, &ptdata)) {
            continue;
        }

        for(index = 0;; index += 1) {
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

            if(blk_device_register_part(&part->base, device, info.part_index)) {
                kfree(part);
                continue;
            }

            part->next = state->parts;
            state->parts = part;

            blk_event_notify(BLK_EVENT_PARTITION_ADDED, &part->base);
        }

        it->ops->release(ptdata);

        return;
    }
}

static void forget_device(const struct blk_device* restrict device)
{
    struct blk_disk_state* it;
    struct blk_disk_state* prev = nullptr;

    for(it = disk_state_head; it; it = it->next) {
        if(it->disk == device) {
            disk_state_remove_parts(it);

            if(prev) {
                prev->next = it->next;
            }
            else {
                disk_state_head = it->next;
            }

            kfree(it);
            return;
        }

        prev = it;
    }
}

static void handle_blk_event(unsigned event, const struct blk_device* restrict device, void* restrict ctxdata)
{
    if(device->flags & BLK_FLAG_PARTITION) {
        return;
    }

    switch(event) {
        case BLK_EVENT_MEDIA_CONNECTED:
            probe_device(device);
            break;

        case BLK_EVENT_MEDIA_DISCONNECTED:
            forget_device(device);
            break;
    }
}

int blk_ptable_register(struct blk_ptable* restrict ptable)
{
    struct blk_ptable* it;

    for(it = ptable_head; it; it = it->priv.next) {
        if(it == ptable) {
            return -EEXIST;
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

    return -ENODEV;
}

int blk_ptable_rescan(const struct blk_device* restrict device)
{
    struct blk_disk_state* state;

    if(device->flags & BLK_FLAG_PARTITION) {
        return -EINVAL;
    }

    state = disk_state_find(device);

    if(state) {
        disk_state_remove_parts(state);
    }

    probe_device(device);

    return 0;
}

define_initcall(blk_ptable, INITCALL_STAGE_BSP_DEV, INITCALL_ORDER_FIRST)
{
    blk_event_register_handler(&handle_blk_event, nullptr);
}
