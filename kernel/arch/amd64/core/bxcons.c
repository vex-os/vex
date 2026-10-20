#include <arch/amd64.pmio.h>
#include <vex/console.h>
#include <vex/initcall.h>
#include <vex/spinlock.h>

static spinlock_t bxcons_lock = SPINLOCK_INIT;

static void bxcons_write(console_t* restrict cons, const void* restrict data, size_t size)
{
    const char* psz = data;

    bool intr_state = spinlock_acquire_intr(&bxcons_lock);

    for(size_t i = 0; i < size; ++i) {
        port_write8(0xE9, psz[i]);
    }

    spinlock_release_intr(&bxcons_lock, intr_state);
}

static const console_ops_t bxcons_ops = {
    .write = &bxcons_write,
};

static console_t bxcons_driver = {
    .ops = &bxcons_ops,
    .name = "bxcons",
    .consflags = CONSF_RECALL,
};

static void arch_init_bxcons(void)
{
    if(0xE9 == port_read8(0xE9)) {
        console_add(&bxcons_driver);
    }
}

initcall_bsp_early(arch_init_bxcons, INIT_FIRST);
