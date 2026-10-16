#include <vex/atomic.h>
#include <vex/chr_name.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/panic.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/strings.h>

static spinlock_type chr_name_lock = SPINLOCK_INIT;
static struct chr_name* chr_name_head = nullptr;

struct chr_name* chr_name_tty;
struct chr_name* chr_name_pty;
struct chr_name* chr_name_fbd;

static struct chr_name* find_name_nolock(const char* restrict prefix)
{
    for(struct chr_name* name = chr_name_head; name; name = name->priv.next) {
        if(0 == strcmp(name->device_prefix, prefix)) {
            return name;
        }
    }

    return nullptr;
}

struct chr_name* chr_name_query(const char* restrict prefix)
{
    spinlock_acquire(&chr_name_lock);
    struct chr_name* name = find_name_nolock(prefix);
    spinlock_release(&chr_name_lock);

    if(name) {
        return name;
    }

    struct chr_name* created = kmalloc(sizeof(struct chr_name));

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

unsigned chr_name_increment(struct chr_name* restrict name)
{
    return atomic_fetch_add(&name->priv.next_iterator, 1, ATOMIC_RELAXED);
}

unsigned chr_name_count(const struct chr_name* restrict name)
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
