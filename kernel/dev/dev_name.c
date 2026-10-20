#include <vex/assert.h>
#include <vex/crc32_iso3309.h>
#include <vex/dev.h>
#include <vex/dev_name.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/panic.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/sprintf.h>
#include <vex/string.h>

#define MAX_UNIT 0xFFF // What the unit field of a dev_t holds

static_assert(sizeof(dev_unit_t) <= SLAB_MAX_SIZE);

static spinlock_t dev_units_lock = SPINLOCK_INIT;

dev_prefix_t dev_prefix_anon = {
    .string = "anon",
};

dev_prefix_t dev_prefix_ata = {
    .string = "ada",
};

dev_prefix_t dev_prefix_atapi = {
    .string = "acd",
};

dev_prefix_t dev_prefix_nvme = {
    .string = "nvd",
};

dev_prefix_t dev_prefix_virtio = {
    .string = "vbd",
};

dev_prefix_t dev_prefix_tty = {
    .string = "tty",
};

dev_prefix_t dev_prefix_pty = {
    .string = "pty",
};

dev_prefix_t dev_prefix_fbd = {
    .string = "fbd",
};

static dev_prefix_t* const dev_prefixes[] = {
    &dev_prefix_anon,
    &dev_prefix_ata,
    &dev_prefix_atapi,
    &dev_prefix_nvme,
    &dev_prefix_virtio,
    &dev_prefix_tty,
    &dev_prefix_pty,
    &dev_prefix_fbd,
};

static void link_unit_locked(dev_prefix_t* restrict prefix, dev_unit_t* restrict unit, dev_unit_t* restrict prev, unsigned number)
{
    unit->prefix = prefix;
    unit->number = number;
    unit->prev = prev;
    unit->next = prev ? prev->next : prefix->units;

    if(unit->next) {
        unit->next->prev = unit;
    }

    if(prev) {
        prev->next = unit;
    }
    else {
        prefix->units = unit;
    }
}

int dev_unit_alloc(dev_prefix_t* restrict prefix, dev_unit_t** restrict unit)
{
    dev_unit_t* created = slab_alloc(sizeof(dev_unit_t));

    if(created == nullptr) {
        return -ENOMEM;
    }

    spinlock_acquire(&dev_units_lock);

    unsigned number = 0;
    dev_unit_t* prev = nullptr;

    for(dev_unit_t* it = prefix->units; it && it->number == number; it = it->next) {
        prev = it;
        number += 1;
    }

    if(number > MAX_UNIT) {
        spinlock_release(&dev_units_lock);
        slab_free(created);
        return -ENOSPC;
    }

    link_unit_locked(prefix, created, prev, number);

    spinlock_release(&dev_units_lock);

    *unit = created;

    return 0;
}

int dev_unit_claim(dev_prefix_t* restrict prefix, unsigned number, dev_unit_t** restrict unit)
{
    if(number > MAX_UNIT) {
        return -ERANGE;
    }

    dev_unit_t* created = slab_alloc(sizeof(dev_unit_t));

    if(created == nullptr) {
        return -ENOMEM;
    }

    spinlock_acquire(&dev_units_lock);

    dev_unit_t* prev = nullptr;

    for(dev_unit_t* it = prefix->units; it && it->number <= number; it = it->next) {
        if(it->number == number) {
            spinlock_release(&dev_units_lock);
            slab_free(created);
            return -EEXIST;
        }

        prev = it;
    }

    link_unit_locked(prefix, created, prev, number);

    spinlock_release(&dev_units_lock);

    *unit = created;

    return 0;
}

void dev_unit_free(dev_unit_t* restrict unit)
{
    spinlock_acquire(&dev_units_lock);

    if(unit->next) {
        unit->next->prev = unit->prev;
    }

    if(unit->prev) {
        unit->prev->next = unit->next;
    }
    else {
        unit->prefix->units = unit->next;
    }

    spinlock_release(&dev_units_lock);

    slab_free(unit);
}

int dev_name_format(const dev_name_t* restrict name, char* restrict buffer, size_t size)
{
    kassert(name->index != DEV_NAME_NONE || name->partition == DEV_NAME_NONE);

    if(name->index != DEV_NAME_NONE && name->index >= DEV_NO_INDEX) {
        return -ERANGE;
    }

    if(name->partition != DEV_NAME_NONE && name->partition >= DEV_NO_PARTITION) {
        return -ERANGE;
    }

    int length;

    if(name->index == DEV_NAME_NONE) {
        length = snprintf(buffer, size, "%s%u", name->unit->prefix->string, name->unit->number);
    }
    else if(name->partition == DEV_NAME_NONE) {
        length = snprintf(buffer, size, "%s%un%u", name->unit->prefix->string, name->unit->number, name->index);
    }
    else {
        length = snprintf(buffer, size, "%s%un%up%u", name->unit->prefix->string, name->unit->number, name->index, name->partition);
    }

    if(length < 0 || (size_t)(length) >= size) {
        return -ENAMETOOLONG;
    }

    return 0;
}

dev_t dev_name_to_dev(const dev_name_t* restrict name)
{
    unsigned index = name->index == DEV_NAME_NONE ? DEV_NO_INDEX : name->index;
    unsigned partition = name->partition == DEV_NAME_NONE ? DEV_NO_PARTITION : name->partition;
    return DEV_SET(name->unit->prefix->hash, name->unit->number, index, partition);
}

static void init_dev_prefixes(void)
{
    for(size_t i = 0; i < sizeof(dev_prefixes) / sizeof(dev_prefixes[0]); ++i) {
        dev_prefix_t* prefix = dev_prefixes[i];
        prefix->hash = crc32_iso3309_compute(prefix->string, strlen(prefix->string));

        for(size_t j = 0; j < i; ++j) {
            if(dev_prefixes[j]->hash == prefix->hash) {
                panic("dev_name: %s and %s have the same hash", dev_prefixes[j]->string, prefix->string);
            }
        }
    }
}

initcall_bsp_early(init_dev_prefixes, INIT_LATE);
