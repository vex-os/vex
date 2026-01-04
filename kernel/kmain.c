#include <arch/init.h>

#include <vex/acpi.h>
#include <vex/devinit.h>
#include <vex/fsinit.h>
#include <vex/hhdm.h>
#include <vex/kbase.h>
#include <vex/kprintf.h>
#include <vex/limine.h>
#include <vex/madt.h>
#include <vex/memmap.h>
#include <vex/panic.h>
#include <vex/pmm.h>
#include <vex/slab.h>
#include <vex/version.h>
#include <vex/vmm.h>

void __noreturn __used kmain(void)
{
    kprintf(KP_INFORM, "%s %s %s", k_sysname, k_release, k_version);

    init_arch_early();

    init_hhdm();
    init_kbase();
    init_memmap();

    init_arch();

    init_pmm();
    init_slab();

    void* test1 = slab_alloc(sizeof(uint16_t));
    void* test2 = slab_alloc(sizeof(uint32_t));
    void* test3 = slab_alloc(sizeof(uint64_t));
    void* test4 = slab_alloc(4 * sizeof(uint64_t));

    kprintf(KP_INFORM, "test 1: %p", test1);
    kprintf(KP_INFORM, "test 2: %p", test2);
    kprintf(KP_INFORM, "test 3: %p", test3);
    kprintf(KP_INFORM, "test 4: %p", test4);

    slab_free(test1);

    (void)(slab_alloc(sizeof(uint16_t)));
    test1 = slab_alloc(sizeof(uint16_t));
    kprintf(KP_INFORM, "test 5: %p", test1);

    init_vmm();

    init_acpi();
    init_madt();

    // Test - iterate through MADT
    const struct madt_header* madt_entry;
    const void* madt_entry_itr = madt_entries;
    do {
        madt_entry = madt_entry_itr;
        kprintf(KP_INFORM, "MADT[%p] %02zX", madt_entry_itr, (size_t)madt_entry->type);
    } while((madt_entry_itr = madt_next_entry(madt_entry_itr)) != NULL);

    init_device_drivers();
    init_filesys_drivers();

    panic("main: nothing else to do");
    unreachable();
}
