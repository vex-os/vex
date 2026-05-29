// SPDX-License-Identifier: BSD-3-Clause
#ifndef D6DE57CA_EA06_4C53_BB52_6B8E263AC6DF
#define D6DE57CA_EA06_4C53_BB52_6B8E263AC6DF
#pragma once

#include <core/cdefs.h>

#define INITFN_STAGE_BSP_EARLY    "00000000" ///< Very early initialization
#define INITFN_STAGE_BSP_ARCH     "01000000" ///< Arch-specific initialization (eg. GDT on AMD64)
#define INITFN_STAGE_BSP_BOOTINFO "02000000" ///< Bootloader-provided data (memory map, HHDM, kernel base)
#define INITFN_STAGE_BSP_PMM      "10000000" ///< Physical memory management
#define INITFN_STAGE_BSP_SLAB     "11000000" ///< Internal dynamic memory management for sub-page-size allocations
#define INITFN_STAGE_BSP_VMM      "12000000" ///< Virtual memory management
#define INITFN_STAGE_BSP_HEAP     "13000000" ///< Kernel heap allocator
#define INITFN_STAGE_BSP_HART     "20000000" ///< Per-CPU data and feature detection for BSP
#define INITFN_STAGE_BSP_IREQ     "21000000" ///< Interrupt controller (I/O APIC, GIC, etc.)
#define INITFN_STAGE_BSP_TIMER    "22000000" ///< Global timer and clocksource (HPET, ACPI PM)
#define INITFN_STAGE_BSP_MP       "23000000" ///< Wake up AP cores
#define INITFN_STAGE_BSP_BUS      "30000000" ///< Bus enumeration (ACPI, PCI, etc.)
#define INITFN_STAGE_BSP_VFS      "31000000" ///< Virtual filesystem and pseudo-filesystems like devfs
#define INITFN_STAGE_BSP_DEV      "32000000" ///< Device drivers
#define INITFN_STAGE_BSP_FS       "33000000" ///< Actual filesystem drivers
#define INITFN_STAGE_BSP_CPUDEV   "34000000" ///< Per-CPU device expansion (eg. NVMe queue affinity)
#define INITFN_STAGE_BSP_SERVICES "40000000" ///< OS-level subsystems (IPC, sockets, etc.)
#define INITFN_STAGE_BSP_LATE     "ff000000" ///< Anything that can wait until the end of kernel initialization

#define INITFN_STAGE_MP_ARCH  "01000000" ///< Arch-specific per-CPU init (GDT/IDT load, TSS)
#define INITFN_STAGE_MP_HART  "20000000" ///< Per-CPU data and feature detection
#define INITFN_STAGE_MP_IREQ  "21000000" ///< Local interrupt controller (LAPIC, etc.)
#define INITFN_STAGE_MP_TIMER "22000000" ///< Per-CPU timer (LAPIC timer)

#define INITFN_ORDER_FIRST  "0"
#define INITFN_ORDER_EARLY  "3"
#define INITFN_ORDER_MIDDLE "5"
#define INITFN_ORDER_LATE   "7"
#define INITFN_ORDER_LAST   "9"

typedef int (*initfn_func_type)(void);
typedef char initfn_name_type[64];

struct initfn_entry {
    initfn_name_type name;
    initfn_func_type pfn;
};

#define DEFINE_INITFN_BSP(fn, stage, order)                                                             \
    static const __used __section(".initfn.bsp." stage order) struct initfn_entry __initfn_bsp_##fn = { \
        .name = #fn,                                                                                    \
        .pfn = fn,                                                                                      \
    }

#define DEFINE_INITFN_MP(fn, stage, order)                                                             \
    static const __used __section(".initfn.mp." stage order) struct initfn_entry __initfn_smp_##fn = { \
        .name = #fn,                                                                                   \
        .pfn = fn,                                                                                     \
    }

extern const struct initfn_entry initfns_bsp_start[];
extern const struct initfn_entry initfns_bsp_end[];

extern const struct initfn_entry initfns_mp_start[];
extern const struct initfn_entry initfns_mp_end[];

extern void call_initfns(const struct initfn_entry* restrict start, const struct initfn_entry* restrict end);

#endif /* D6DE57CA_EA06_4C53_BB52_6B8E263AC6DF */
