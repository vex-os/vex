// SPDX-License-Identifier: BSD-3-Clause
#ifndef D6DE57CA_EA06_4C53_BB52_6B8E263AC6DF
#define D6DE57CA_EA06_4C53_BB52_6B8E263AC6DF
#pragma once

#include <core/cdefs.h>

#define INITFN_CAT_EARLY      "01000000" ///< Very early initialization
#define INITFN_CAT_ARCH       "02000000" ///< Arch-specific initialization (eg. GDT on AMD64)
#define INITFN_CAT_SANITIZERS "04000000" ///< KASAN, UBSAN and similar instrumentation
#define INITFN_CAT_PMM        "10000000" ///< Physical memory management
#define INITFN_CAT_SLAB       "20000000" ///< Internal dynamic memory management for sub-page-size allocations
#define INITFN_CAT_VMM        "40000000" ///< Virtual memory management
#define INITFN_CAT_LOCKS      "50000000" ///< Synchronization primitives
#define INITFN_CAT_HART       "58000000" ///< Per-CPU data and feature detection
#define INITFN_CAT_SMP        "60000000" ///< Other cores wake up
#define INITFN_CAT_IREQ       "68000000" ///< Interrupt controller (APIC, GIC, etc.)
#define INITFN_CAT_TIMER      "70000000" ///< System timer and clocksource
#define INITFN_CAT_BUS        "78000000" ///< Bus enumeration (ACPI, PCI, etc.)
#define INITFN_CAT_DEV        "88000000" ///< Device drivers
#define INITFN_CAT_FS         "98000000" ///< VFS and filesystem drivers
#define INITFN_CAT_SERVICES   "a8000000" ///< OS-level subsystems (IPC, sockets, etc.)
#define INITFN_CAT_LATE       "f0000000" ///< Anything that can wait until the end of kernel initialization

#define INITFN_ORDER_FIRST  "0"
#define INITFN_ORDER_EARLY  "3"
#define INITFN_ORDER_MIDDLE "5"
#define INITFN_ORDER_LATE   "7"
#define INITFN_ORDER_LAST   "9"

typedef int (*initfn_pfn_t)(void);
typedef char initfn_name_t[64];

struct initfn_entry {
    initfn_name_t name;
    initfn_pfn_t pfn;
};

#define DEFINE_INITFN(fn, cat, order)                                                         \
    static const __used __section(".initfn." cat order) struct initfn_entry __initfn_##fn = { \
        .name = #fn,                                                                          \
        .pfn = fn,                                                                            \
    }

extern const struct initfn_entry initfns_start[];
extern const struct initfn_entry initfns_end[];

#endif /* D6DE57CA_EA06_4C53_BB52_6B8E263AC6DF */
