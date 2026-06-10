#include <vex.arch/hart.h>
#include <vex.x86/gdt.h>
#include <vex.x86/tss.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/panic.h>
#include <vex/string.h>


static void arch_init_tss(void)
{
    struct x86_tss* tss = kmalloc(sizeof(struct x86_tss));

    if(tss == nullptr) {
        panic("out of memory");
    }

    tss->iomap_base = sizeof(*tss);

    struct hart_local* hart = this_hart();

    for(size_t i = 0; i < AMD64_IST_COUNT; ++i) {
        if(hart->x86_ist_stacks[i]) {
            tss->ist[i] = hart->x86_ist_stacks[i];
        }
    }

    hart->x86_tssp = tss;

    install_tss(hart->x86_gdtp, (uintptr_t)(tss), sizeof(struct x86_tss));

    asm volatile("ltr %0" ::"r"(gdt_selector(GDT_TSS, 0, 0)));
}

initcall_smp_arch(arch_init_tss, INIT_MIDDLE);
