#include <vex/blk_device.h>
#include <vex/blk_event.h>
#include <vex/blk_ptable.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>

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

static void part_submit(const struct blk_device* restrict device, struct blk_request* restrict req)
{
    const struct blk_part_device* part = (const struct blk_part_device*)(device);

    if(req->op != BLK_REQ_FLUSH) {
        req->lba += part->lba_start;
    }

    blk_submit(part->root, req);
}

static void part_release(const struct blk_device* restrict device)
{
    const struct blk_part_device* part = (const struct blk_part_device*)(device);

    blk_device_decref(part->root);
    kfree((void*)(device));
}

static const struct blk_device_ops part_ops = {
    .submit = &part_submit,
    .release = &part_release,
};

static struct blk_disk_state* disk_state_find(const struct blk_device* restrict disk)
{
    struct blk_disk_state* it;

    for(it = disk_state_head; it; it = it->next) {
        if(it->disk == disk) {
            return it;
        }
    }

    return nullptr;
}

static void disk_state_remove_parts(struct blk_disk_state* restrict state)
{
    for(struct blk_part_device* it = state->parts; it;) {
        struct blk_part_device* next = it->next;

        blk_event_notify(BLK_EVENT_PARTITION_REMOVED, &it->base);

        remove_blk_device(&it->base);

        it = next;
    }

    state->parts = nullptr;
}

static void probe_device(const struct blk_device* restrict device)
{
    struct blk_disk_state* state = disk_state_find(device);

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

    for(struct blk_ptable* it = ptable_head; it; it = it->priv.next) {
        void* ptdata = nullptr;

        if(it->ops->detect(device, &ptdata)) {
            continue;
        }

        for(unsigned index = 0;; index += 1) {
            struct blk_ptable_info info;
            if(it->ops->readpt(device, ptdata, index, &info)) {
                break;
            }

            struct blk_part_device* part = kmalloc(sizeof(struct blk_part_device));

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
            part->base.max_transfer = device->max_transfer;
            part->base.max_segments = device->max_segments;
            part->base.dma_limit = device->dma_limit;
            part->base.dma_align = device->dma_align;
            part->base.flags = BLK_FLAG_PARTITION | (device->flags & BLK_FLAG_PRESENT);
            part->base.ctxdata = nullptr;

            if(add_blk_device_part(&part->base, device, info.part_index)) {
                kfree(part);
                continue;
            }

            blk_device_incref(device); // Dropped by part_release

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

int add_blk_ptable(struct blk_ptable* restrict ptable)
{
    blk_event_lock();

    for(struct blk_ptable* it = ptable_head; it; it = it->priv.next) {
        if(it == ptable) {
            blk_event_unlock();
            return -EEXIST;
        }
    }

    ptable->priv.next = ptable_head;
    ptable_head = ptable;

    blk_event_unlock();

    return 0;
}

int remove_blk_ptable(struct blk_ptable* restrict ptable)
{
    blk_event_lock();

    for(struct blk_ptable** prev = &ptable_head; *prev; prev = &(*prev)->priv.next) {
        if(*prev == ptable) {
            *prev = ptable->priv.next;
            blk_event_unlock();
            return 0;
        }
    }

    blk_event_unlock();

    return -ENODEV;
}

int rescan_partitions(const struct blk_device* restrict device)
{
    if(device->flags & BLK_FLAG_PARTITION) {
        return -EINVAL;
    }

    blk_event_lock();

    struct blk_disk_state* state = disk_state_find(device);

    if(state) {
        disk_state_remove_parts(state);
    }

    probe_device(device);

    blk_event_unlock();

    return 0;
}

static void init_blk_ptable(void)
{
    blk_event_subscribe(&handle_blk_event, nullptr);
}

initcall_bsp_dev(init_blk_ptable, INIT_FIRST);
