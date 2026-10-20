#pragma once

#include <vex/compiler.h>

#define INIT_FIRST  ".0"
#define INIT_EARLY  ".3"
#define INIT_MIDDLE ".5"
#define INIT_LATE   ".7"
#define INIT_LAST   ".9"

typedef struct initcall_s initcall_t;

struct initcall_s {
    void (*function)(void);
    const char* name;
};

#define __define_initcall(ic_function, ic_section) \
    static const __used __section(ic_section)      \
    initcall_t __initcall_##ic_function = {        \
        .function = &(ic_function),                \
        .name = #ic_function,                      \
    }

#define initcall_bsp_early(ic_function, ic_order)     __define_initcall(ic_function, ".initcalls.bsp.0000000" ic_order)
#define initcall_bsp_arch(ic_function, ic_order)      __define_initcall(ic_function, ".initcalls.bsp.0100000" ic_order)
#define initcall_bsp_bootinfo(ic_function, ic_order)  __define_initcall(ic_function, ".initcalls.bsp.0200000" ic_order)
#define initcall_bsp_pmm(ic_function, ic_order)       __define_initcall(ic_function, ".initcalls.bsp.1000000" ic_order)
#define initcall_bsp_slab(ic_function, ic_order)      __define_initcall(ic_function, ".initcalls.bsp.1100000" ic_order)
#define initcall_bsp_mmu(ic_function, ic_order)       __define_initcall(ic_function, ".initcalls.bsp.1200000" ic_order)
#define initcall_bsp_heap(ic_function, ic_order)      __define_initcall(ic_function, ".initcalls.bsp.1300000" ic_order)
#define initcall_bsp_irq(ic_function, ic_order)       __define_initcall(ic_function, ".initcalls.bsp.2000000" ic_order)
#define initcall_bsp_timer(ic_function, ic_order)     __define_initcall(ic_function, ".initcalls.bsp.2100000" ic_order)
#define initcall_bsp_syscall(ic_function, ic_order)   __define_initcall(ic_function, ".initcalls.bsp.2200000" ic_order)
#define initcall_bsp_multicore(ic_function, ic_order) __define_initcall(ic_function, ".initcalls.bsp.2300000" ic_order)
#define initcall_bsp_bus(ic_function, ic_order)       __define_initcall(ic_function, ".initcalls.bsp.3000000" ic_order)
#define initcall_bsp_vfs(ic_function, ic_order)       __define_initcall(ic_function, ".initcalls.bsp.3100000" ic_order)
#define initcall_bsp_dev(ic_function, ic_order)       __define_initcall(ic_function, ".initcalls.bsp.3200000" ic_order)
#define initcall_bsp_fs(ic_function, ic_order)        __define_initcall(ic_function, ".initcalls.bsp.3300000" ic_order)
#define initcall_bsp_cpudev(ic_function, ic_order)    __define_initcall(ic_function, ".initcalls.bsp.3400000" ic_order)
#define initcall_bsp_binfmt(ic_function, ic_order)    __define_initcall(ic_function, ".initcalls.bsp.3500000" ic_order)
#define initcall_bsp_services(ic_function, ic_order)  __define_initcall(ic_function, ".initcalls.bsp.4000000" ic_order)
#define initcall_bsp_late(ic_function, ic_order)      __define_initcall(ic_function, ".initcalls.bsp.ff00000" ic_order)

#define initcall_smp_hart(ic_function, ic_order)    __define_initcall(ic_function, ".initcalls.smp.0000000" ic_order)
#define initcall_smp_arch(ic_function, ic_order)    __define_initcall(ic_function, ".initcalls.smp.0100000" ic_order)
#define initcall_smp_irq(ic_function, ic_order)     __define_initcall(ic_function, ".initcalls.smp.2100000" ic_order)
#define initcall_smp_syscall(ic_function, ic_order) __define_initcall(ic_function, ".initcalls.smp.2200000" ic_order)
#define initcall_smp_timer(ic_function, ic_order)   __define_initcall(ic_function, ".initcalls.smp.2300000" ic_order)
#define initcall_smp_late(ic_function, ic_order)    __define_initcall(ic_function, ".initcalls.smp.ff00000" ic_order)

extern const initcall_t initcalls_bsp_start[];
extern const initcall_t initcalls_bsp_end[];

extern const initcall_t initcalls_smp_start[];
extern const initcall_t initcalls_smp_end[];

void invoke_initcalls(const initcall_t* restrict start, const initcall_t* restrict end);
