#include <string.h>
#include <sys.amd64/gdt.h>
#include <sys.amd64/tss.h>
#include <sys.arch/hart.h>
#include <sys/initcall.h>
#include <sys/kprintf.h>
#include <sys/malloc.h>
#include <sys/panic.h>

define_initcall(amd64_tss, INITCALL_STAGE_SMP_ARCH, INIT_MIDDLE)
{
    struct amd64_tss* tss = kmalloc(sizeof(struct amd64_tss));

    if(tss == nullptr) {
        panic("out of memory");
    }

    tss->iomap_base = sizeof(*tss);

    struct hart_local* hart = this_hart();

    for(size_t i = 0; i < AMD64_IST_COUNT; ++i) {
        if(hart->amd64.ist_stacks[i]) {
            tss->ist[i] = hart->amd64.ist_stacks[i];
        }
    }

    hart->amd64.tss = tss;

    install_tss(hart->amd64.gdt, (uintptr_t)(tss), sizeof(struct amd64_tss));

    asm volatile("ltr %0" ::"r"(gdt_selector(GDT_TSS, 0, 0)));
}
