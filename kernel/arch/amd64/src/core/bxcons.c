#include <arch/amd64.pmio.h>
#include <vex/console.h>
#include <vex/initcall.h>
#include <vex/spinlock.h>

static spinlock_type bxcons_lock = SPINLOCK_INIT;

static void bxcons_write(struct console* restrict cons, const void* restrict data, size_t size)
{
    const char* psz = data;

    bool intr_state = spinlock_acquire_intr(&bxcons_lock);

    for(size_t i = 0; i < size; ++i) {
        pmwrite8(0xE9, psz[i]);
    }

    spinlock_release_intr(&bxcons_lock, intr_state);
}

static const struct console_ops bxcons_ops = {
    .write = &bxcons_write,
};

static struct console bxcons_driver = {
    .ops = &bxcons_ops,
    .identifier = "bxcons",
    .consflags = CONSF_RECALL,
};

static void arch_init_bxcons(void)
{
    if(0xE9 == pmread8(0xE9)) {
        add_console(&bxcons_driver);
    }
}

initcall_bsp_early(arch_init_bxcons, INIT_FIRST);
