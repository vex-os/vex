#include <vex/blk_device.h>
#include <vex/blk_event.h>
#include <vex/blk_ptable.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>

typedef struct blk_part_device_s blk_part_device_t;
typedef struct blk_disk_state_s blk_disk_state_t;

struct blk_part_device_s {
    blk_device_t base;
    blk_part_device_t* next;
    const blk_device_t* root;
    uintptr_t lba_start;
    size_t lba_count;
};

struct blk_disk_state_s {
    blk_disk_state_t* next;
    const blk_device_t* disk;
    blk_part_device_t* parts;
};

static blk_partition_table_t* ptable_head = nullptr;
static blk_disk_state_t* disk_state_head = nullptr;

static void part_submit(const blk_device_t* restrict device, blk_request_t* restrict req)
{
    const blk_part_device_t* part = (const blk_part_device_t*)(device);

    if(req->op != BLK_REQ_FLUSH) {
        req->lba += part->lba_start;
    }

    blk_submit(part->root, req);
}

static void part_release(const blk_device_t* restrict device)
{
    const blk_part_device_t* part = (const blk_part_device_t*)(device);

    blk_device_decref(part->root);
    kfree((void*)(device));
}

static const blk_device_ops_t part_ops = {
    .submit = &part_submit,
    .release = &part_release,
};

static blk_disk_state_t* disk_state_find(const blk_device_t* restrict disk)
{
    blk_disk_state_t* it;

    for(it = disk_state_head; it; it = it->next) {
        if(it->disk == disk) {
            return it;
        }
    }

    return nullptr;
}

static void disk_state_remove_parts(blk_disk_state_t* restrict state)
{
    for(blk_part_device_t* it = state->parts; it;) {
        blk_part_device_t* next = it->next;

        blk_event_notify(BLK_EVENT_PARTITION_REMOVED, &it->base);

        blk_device_remove(&it->base);

        it = next;
    }

    state->parts = nullptr;
}

static void probe_device(const blk_device_t* restrict device)
{
    blk_disk_state_t* state = disk_state_find(device);

    if(state == nullptr) {
        state = kmalloc(sizeof(blk_disk_state_t));

        if(state == nullptr) {
            return;
        }

        state->disk = device;
        state->parts = nullptr;

        state->next = disk_state_head;
        disk_state_head = state;
    }

    for(blk_partition_table_t* it = ptable_head; it; it = it->priv.next) {
        void* ptdata = nullptr;

        if(it->ops->detect(device, &ptdata)) {
            continue;
        }

        for(unsigned index = 0;; index += 1) {
            blk_partition_table_info_t info;
            if(it->ops->readpt(device, ptdata, index, &info)) {
                break;
            }

            blk_part_device_t* part = kmalloc(sizeof(blk_part_device_t));

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

            if(blk_device_add_part(&part->base, device, info.part_index)) {
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

static void forget_device(const blk_device_t* restrict device)
{
    blk_disk_state_t* it;
    blk_disk_state_t* prev = nullptr;

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

static void handle_blk_event(unsigned event, const blk_device_t* restrict device, void* restrict ctxdata)
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

int blk_partition_table_add(blk_partition_table_t* restrict ptable)
{
    blk_event_lock();

    for(blk_partition_table_t* it = ptable_head; it; it = it->priv.next) {
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

int blk_partition_table_remove(blk_partition_table_t* restrict ptable)
{
    blk_event_lock();

    for(blk_partition_table_t** prev = &ptable_head; *prev; prev = &(*prev)->priv.next) {
        if(*prev == ptable) {
            *prev = ptable->priv.next;
            blk_event_unlock();
            return 0;
        }
    }

    blk_event_unlock();

    return -ENODEV;
}

int rescan_partitions(const blk_device_t* restrict device)
{
    if(device->flags & BLK_FLAG_PARTITION) {
        return -EINVAL;
    }

    blk_event_lock();

    blk_disk_state_t* state = disk_state_find(device);

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
