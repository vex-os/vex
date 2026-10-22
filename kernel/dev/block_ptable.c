#include <vex/block_device.h>
#include <vex/block_event.h>
#include <vex/block_ptable.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>

typedef struct block_part_device_s block_part_device_t;
typedef struct block_disk_state_s block_disk_state_t;

struct block_part_device_s {
    block_device_t base;
    block_part_device_t* next;
    const block_device_t* root;
    uintptr_t lba_start;
    size_t lba_count;
};

struct block_disk_state_s {
    block_disk_state_t* next;
    const block_device_t* disk;
    block_part_device_t* parts;
};

static block_partition_table_t* ptable_head = nullptr;
static block_disk_state_t* disk_state_head = nullptr;

static void part_submit(const block_device_t* restrict device, block_request_t* restrict req)
{
    const block_part_device_t* part = (const block_part_device_t*)(device);

    if(req->op != BLOCK_REQ_FLUSH) {
        req->lba += part->lba_start;
    }

    block_submit(part->root, req);
}

static void part_release(const block_device_t* restrict device)
{
    const block_part_device_t* part = (const block_part_device_t*)(device);

    block_device_decref(part->root);
    kfree((void*)(device));
}

static const block_device_ops_t part_ops = {
    .submit = &part_submit,
    .release = &part_release,
};

static block_disk_state_t* disk_state_find(const block_device_t* restrict disk)
{
    block_disk_state_t* it;

    for(it = disk_state_head; it; it = it->next) {
        if(it->disk == disk) {
            return it;
        }
    }

    return nullptr;
}

static void disk_state_remove_parts(block_disk_state_t* restrict state)
{
    for(block_part_device_t* it = state->parts; it;) {
        block_part_device_t* next = it->next;

        block_event_notify(BLOCK_EVENT_PARTITION_REMOVED, &it->base);

        block_device_remove(&it->base);

        it = next;
    }

    state->parts = nullptr;
}

static void probe_device(const block_device_t* restrict device)
{
    block_disk_state_t* state = disk_state_find(device);

    if(state == nullptr) {
        state = kmalloc(sizeof(block_disk_state_t));

        if(state == nullptr) {
            return;
        }

        state->disk = device;
        state->parts = nullptr;

        state->next = disk_state_head;
        disk_state_head = state;
    }

    for(block_partition_table_t* it = ptable_head; it; it = it->priv.next) {
        void* ptdata = nullptr;

        if(it->ops->detect(device, &ptdata)) {
            continue;
        }

        for(unsigned index = 0;; index += 1) {
            block_partition_table_info_t info;
            if(it->ops->read_table(device, ptdata, index, &info)) {
                break;
            }

            block_part_device_t* part = kmalloc(sizeof(block_part_device_t));

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
            part->base.flags = BLOCK_FLAG_PARTITION | (device->flags & BLOCK_FLAG_PRESENT);
            part->base.ctxdata = nullptr;

            if(block_device_add_part(&part->base, device, info.part_index + 1)) {
                kfree(part);
                continue;
            }

            block_device_incref(device); // Dropped by part_release

            part->next = state->parts;
            state->parts = part;

            block_event_notify(BLOCK_EVENT_PARTITION_ADDED, &part->base);
        }

        it->ops->release(ptdata);

        return;
    }
}

static void forget_device(const block_device_t* restrict device)
{
    block_disk_state_t* it;
    block_disk_state_t* prev = nullptr;

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

static void handle_block_event(unsigned event, const block_device_t* restrict device, void* restrict ctxdata)
{
    if(device->flags & BLOCK_FLAG_PARTITION) {
        return;
    }

    switch(event) {
        case BLOCK_EVENT_MEDIA_CONNECTED:
            probe_device(device);
            break;

        case BLOCK_EVENT_MEDIA_DISCONNECTED:
            forget_device(device);
            break;
    }
}

int block_partition_table_add(block_partition_table_t* restrict ptable)
{
    block_event_lock();

    for(block_partition_table_t* it = ptable_head; it; it = it->priv.next) {
        if(it == ptable) {
            block_event_unlock();
            return -EEXIST;
        }
    }

    ptable->priv.next = ptable_head;
    ptable_head = ptable;

    block_event_unlock();

    return 0;
}

int block_partition_table_remove(block_partition_table_t* restrict ptable)
{
    block_event_lock();

    for(block_partition_table_t** prev = &ptable_head; *prev; prev = &(*prev)->priv.next) {
        if(*prev == ptable) {
            *prev = ptable->priv.next;
            block_event_unlock();
            return 0;
        }
    }

    block_event_unlock();

    return -ENODEV;
}

int rescan_partitions(const block_device_t* restrict device)
{
    if(device->flags & BLOCK_FLAG_PARTITION) {
        return -EINVAL;
    }

    block_event_lock();

    block_disk_state_t* state = disk_state_find(device);

    if(state) {
        disk_state_remove_parts(state);
    }

    probe_device(device);

    block_event_unlock();

    return 0;
}

static void init_block_ptable(void)
{
    block_event_subscribe(&handle_block_event, nullptr);
}

initcall_bsp_dev(init_block_ptable, INIT_FIRST);
