#ifndef C02B5460_568D_48F8_B826_99E2AE03A5AC
#define C02B5460_568D_48F8_B826_99E2AE03A5AC
#pragma once

#include <core/cdefs.h>

#define INITCALL_STAGE_BSP_EARLY     ".bsp.0000000" ///< [BSP] Very only initialization, stuff like variable initialization
#define INTICALL_STAGE_BSP_ARCH      ".bsp.0100000" ///< [BSP] Arch-specific initialization (eg. GDT on AMD64)
#define INITCALL_STAGE_BSP_BOOTINFO  ".bsp.0200000" ///< [BSP] Bootloader-provided data (memory map, HHDM, kernel base)
#define INITCALL_STAGE_BSP_PMM       ".bsp.1000000" ///< [BSP] Physical memory management (pdma and pslab)
#define INITCALL_STAGE_BSP_SLAB      ".bsp.1100000" ///< [BSP] Internal dynamic memory management for sub-page-size allocations
#define INITCALL_STAGE_BSP_VMM       ".bsp.1200000" ///< [BSP] Virtual memory management
#define INITCALL_STAGE_BSP_HEAP      ".bsp.1300000" ///< [BSP] Kernel heap allocator
#define INITCALL_STAGE_BSP_IREQ      ".bsp.2000000" ///< [BSP] Interrupt controller (IOAPIC, GIC, etc.)
#define INITCALL_STAGE_BSP_TIMER     ".bsp.2100000" ///< [BSP] Global clocksources initialize (HPET, ACPI PM timer, etc.)
#define INITCALL_STAGE_BSP_MULTICORE ".bsp.2200000" ///< [BSP] After this point, multiple cores are awake and running SMP initcalls
#define INITCALL_STAGE_BSP_BUS       ".bsp.3000000" ///< [BSP] Bus drivers and enumeration (ACPI, PCI, ISA/LPC, etc.)
#define INITCALL_STAGE_BSP_VFS       ".bsp.3100000" ///< [BSP] Virtual filesystem and pseudo-filesystems like devfs
#define INITCALL_STAGE_BSP_DEV       ".bsp.3200000" ///< [BSP] Device drivers
#define INITCALL_STAGE_BSP_FS        ".bsp.3300000" ///< [BSP] Actual filesystem drivers
#define INITCALL_STAGE_BSP_CPUDEV    ".bsp.3400000" ///< [BSP] Device drivers that require per-CPU initialization
#define INITCALL_STAGE_BSP_SERVICES  ".bsp.4000000" ///< [BSP] OS-level subsystems (IPC, sockets, etc.)
#define INITCALL_STAGE_BSP_LATE      ".bsp.ff00000" ///< [BSP] Anything that can wait until the end of kernel initialization

#define INITCALL_STAGE_SMP_HART  ".smp.0000000" ///< [SMP] Per-hart data is initialized (hart_local)
#define INITCALL_STAGE_SMP_ARCH  ".smp.0100000" ///< [SMP] Arch-specific per-hart init (GDT/IDT load, TSS)
#define INITCALL_STAGE_SMP_IREQ  ".smp.2100000" ///< [SMP] Local interrupt controller (LAPIC, etc.)
#define INITCALL_STAGE_SMP_TIMER ".smp.2200000" ///< [SMP] Per-hart clockevents initialized (LAPIC timer, etc.)

#define INITCALL_ORDER_FIRST  ".0"
#define INITCALL_ORDER_EARLY  ".3"
#define INITCALL_ORDER_MIDDLE ".5"
#define INITCALL_ORDER_LATE   ".7"
#define INITCALL_ORDER_LAST   ".9"

typedef void (*initcall_function_type)(void);
typedef char initcall_identifier_type[64];

struct initcall_entry {
    initcall_identifier_type name;
    initcall_function_type func;
};

#define define_initcall(identifier, stage, order)                                                            \
    static void __initcall_wrapper_##identifier(void);                                                       \
    static const __used __section(".initcall" stage order) struct initcall_entry __initcall_##identifier = { \
        .name = #identifier,                                                                                 \
        .func = __initcall_wrapper_##identifier,                                                             \
    };                                                                                                       \
    static void __initcall_wrapper_##identifier(void)

extern const struct initcall_entry initcalls_bsp_start[];
extern const struct initcall_entry initcalls_bsp_end[];

extern const struct initcall_entry initcalls_smp_start[];
extern const struct initcall_entry initcalls_smp_end[];

void invoke_initcalls(const struct initcall_entry* start, const struct initcall_entry* end);

#endif /* C02B5460_568D_48F8_B826_99E2AE03A5AC */
