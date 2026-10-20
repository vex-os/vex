#include <vex/atomic.h>
#include <vex/chr_name.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/panic.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/strings.h>

static spinlock_t chr_name_lock = SPINLOCK_INIT;
static chr_name_t* chr_name_head = nullptr;

chr_name_t* chr_name_tty;
chr_name_t* chr_name_pty;
chr_name_t* chr_name_fbd;

static chr_name_t* find_name_nolock(const char* restrict prefix)
{
    for(chr_name_t* name = chr_name_head; name; name = name->priv.next) {
        if(0 == strcmp(name->device_prefix, prefix)) {
            return name;
        }
    }

    return nullptr;
}

chr_name_t* chr_name_query(const char* restrict prefix)
{
    spinlock_acquire(&chr_name_lock);
    chr_name_t* name = find_name_nolock(prefix);
    spinlock_release(&chr_name_lock);

    if(name) {
        return name;
    }

    chr_name_t* created = kmalloc(sizeof(chr_name_t));

    if(created == nullptr) {
        return nullptr;
    }

    kstrncpy(created->device_prefix, prefix, sizeof(created->device_prefix));
    created->priv.next_iterator = 0;

    spinlock_acquire(&chr_name_lock);

    name = find_name_nolock(prefix);

    if(name == nullptr) {
        created->priv.next = chr_name_head;
        chr_name_head = created;
        name = created;
        created = nullptr;
    }

    spinlock_release(&chr_name_lock);

    kfree(created);

    return name;
}

unsigned chr_name_increment(chr_name_t* restrict name)
{
    return atomic_fetch_add(&name->priv.next_iterator, 1, ATOMIC_RELAXED);
}

unsigned chr_name_count(const chr_name_t* restrict name)
{
    return atomic_load(&name->priv.next_iterator, ATOMIC_RELAXED);
}

static void init_chr_name_predefined(void)
{
    chr_name_tty = chr_name_query(CHR_NAME_PREFIX_TTY);
    chr_name_pty = chr_name_query(CHR_NAME_PREFIX_PTY);
    chr_name_fbd = chr_name_query(CHR_NAME_PREFIX_FBD);

    bool oom = false;
    oom = oom || chr_name_tty == nullptr;
    oom = oom || chr_name_pty == nullptr;
    oom = oom || chr_name_fbd == nullptr;

    if(oom) {
        panic("chr_name: out of memory");
    }
}

initcall_bsp_bus(init_chr_name_predefined, INIT_LATE);
