#include <vex/atomic.h>
#include <vex/blk_name.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/panic.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/strings.h>

static spinlock_type blk_name_lock = SPINLOCK_INIT;
static struct blk_name* blk_name_head = nullptr;

struct blk_name* blk_name_ata;
struct blk_name* blk_name_atapi;
struct blk_name* blk_name_nvme;
struct blk_name* blk_name_virtio;

static struct blk_name* find_name_nolock(const char* restrict prefix)
{
    for(struct blk_name* name = blk_name_head; name; name = name->priv.next) {
        if(0 == strcmp(name->device_prefix, prefix)) {
            return name;
        }
    }

    return nullptr;
}

struct blk_name* blk_name_query(const char* restrict prefix)
{
    spinlock_acquire(&blk_name_lock);
    struct blk_name* name = find_name_nolock(prefix);
    spinlock_release(&blk_name_lock);

    if(name) {
        return name;
    }

    struct blk_name* created = kmalloc(sizeof(struct blk_name));

    if(created == nullptr) {
        return nullptr;
    }

    kstrncpy(created->device_prefix, prefix, sizeof(created->device_prefix));
    created->priv.next_iterator = 0;

    spinlock_acquire(&blk_name_lock);

    name = find_name_nolock(prefix);

    if(name == nullptr) {
        created->priv.next = blk_name_head;
        blk_name_head = created;
        name = created;
        created = nullptr;
    }

    spinlock_release(&blk_name_lock);

    kfree(created);

    return name;
}

unsigned blk_name_increment(struct blk_name* restrict name)
{
    return atomic_fetch_add(&name->priv.next_iterator, 1, ATOMIC_RELAXED);
}

unsigned blk_name_count(const struct blk_name* restrict name)
{
    return atomic_load(&name->priv.next_iterator, ATOMIC_RELAXED);
}

static void init_blk_name_predefined(void)
{
    blk_name_ata = blk_name_query(BLK_NAME_PREFIX_ATA);
    blk_name_atapi = blk_name_query(BLK_NAME_PREFIX_ATAPI);
    blk_name_nvme = blk_name_query(BLK_NAME_PREFIX_NVME);
    blk_name_virtio = blk_name_query(BLK_NAME_PREFIX_VIRTIO);

    bool oom = false;
    oom = oom || blk_name_ata == nullptr;
    oom = oom || blk_name_atapi == nullptr;
    oom = oom || blk_name_nvme == nullptr;
    oom = oom || blk_name_virtio == nullptr;

    if(oom) {
        panic("blk_name: out of memory");
    }
}

initcall_bsp_bus(init_blk_name_predefined, INIT_LATE);
