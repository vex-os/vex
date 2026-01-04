#include <arch/gdt.h>
#include <arch/idt.h>
#include <arch/init.h>
#include <arch/intr.h>
#include <arch/ireq.h>
#include <arch/pic.h>

#if CONF_DEV_BXCONS_ENABLE
#include <arch/bxcons.h>
#endif

void init_arch_early(void)
{
#if CONF_DEV_BXCONS_ENABLE
    init_bxcons();
#endif
}

void init_arch(void)
{
    init_gdt();
    init_idt();
    init_ireq();
    init_pic();
}
