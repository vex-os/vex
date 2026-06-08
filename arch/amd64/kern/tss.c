#include <string.h>
#include <sys.amd64/gdt.h>
#include <sys.amd64/tss.h>
#include <sys.arch/hart.h>
#include <sys/initcall.h>
#include <sys/kprintf.h>
#include <sys/malloc.h>
#include <sys/panic.h>

static void arch_init_tss(void)
{
    struct amd64_tss* tss = kmalloc(sizeof(struct amd64_tss));

    if(tss == nullptr) {
        panic("out of memory");
    }

    tss->iomap_base = sizeof(*tss);

    struct hart_local* hart = this_hart();

    for(size_t i = 0; i < AMD64_IST_COUNT; ++i) {
        if(hart->amd64_ist_stacks[i]) {
            tss->ist[i] = hart->amd64_ist_stacks[i];
        }
    }

    hart->amd64_tssp = tss;

    install_tss(hart->amd64_gdtp, (uintptr_t)(tss), sizeof(struct amd64_tss));

    asm volatile("ltr %0" ::"r"(gdt_selector(GDT_TSS, 0, 0)));
}

initcall_smp_arch(arch_init_tss, INIT_MIDDLE);
