#ifndef C02B5460_568D_48F8_B826_99E2AE03A5AC
#define C02B5460_568D_48F8_B826_99E2AE03A5AC
#pragma once

#include <sys/cdefs.h>

#define INIT_FIRST  ".0"
#define INIT_EARLY  ".3"
#define INIT_MIDDLE ".5"
#define INIT_LATE   ".7"
#define INIT_LAST   ".9"

struct initcall {
    void (*function)(void);
    const char* identifier;
};

#define __define_initcall(ic_function, ic_section) \
    static const __used __section(ic_section)      \
    struct initcall __initcall_##ic_function = {   \
        .function = &(ic_function),                \
        .identifier = #ic_function,                \
    }

#define initcall_bsp_early(ic_function, ic_order)     __define_initcall(ic_function, ".initcalls.bsp.0000000" ic_order)
#define initcall_bsp_arch(ic_function, ic_order)      __define_initcall(ic_function, ".initcalls.bsp.0100000" ic_order)
#define initcall_bsp_bootinfo(ic_function, ic_order)  __define_initcall(ic_function, ".initcalls.bsp.0200000" ic_order)
#define initcall_bsp_pmm(ic_function, ic_order)       __define_initcall(ic_function, ".initcalls.bsp.1000000" ic_order)
#define initcall_bsp_slab(ic_function, ic_order)      __define_initcall(ic_function, ".initcalls.bsp.1100000" ic_order)
#define initcall_bsp_vmm(ic_function, ic_order)       __define_initcall(ic_function, ".initcalls.bsp.1200000" ic_order)
#define initcall_bsp_heap(ic_function, ic_order)      __define_initcall(ic_function, ".initcalls.bsp.1300000" ic_order)
#define initcall_bsp_ireq(ic_function, ic_order)      __define_initcall(ic_function, ".initcalls.bsp.2000000" ic_order)
#define initcall_bsp_timer(ic_function, ic_order)     __define_initcall(ic_function, ".initcalls.bsp.2100000" ic_order)
#define inticall_bsp_syscall(ic_function, ic_order)   __define_initcall(ic_function, ".initcalls.bsp.2200000" ic_order)
#define initcall_bsp_multicore(ic_function, ic_order) __define_initcall(ic_function, ".initcalls.bsp.2300000" ic_order)
#define initcall_bsp_bus(ic_function, ic_order)       __define_initcall(ic_function, ".initcalls.bsp.3000000" ic_order)
#define initcall_bsp_vfs(ic_function, ic_order)       __define_initcall(ic_function, ".initcalls.bsp.3100000" ic_order)
#define initcall_bsp_dev(ic_function, ic_order)       __define_initcall(ic_function, ".initcalls.bsp.3200000" ic_order)
#define initcall_bsp_fs(ic_function, ic_order)        __define_initcall(ic_function, ".initcalls.bsp.3300000" ic_order)
#define initcall_bsp_cpudev(ic_function, ic_order)    __define_initcall(ic_function, ".initcalls.bsp.3400000" ic_order)
#define initcall_bsp_services(ic_function, ic_order)  __define_initcall(ic_function, ".initcalls.bsp.4000000" ic_order)
#define initcall_bsp_late(ic_function, ic_order)      __define_initcall(ic_function, ".initcalls.bsp.ff00000" ic_order)

#define initcall_smp_hart(ic_function, ic_order)    __define_initcall(ic_function, ".initcalls.smp.0000000" ic_order)
#define initcall_smp_arch(ic_function, ic_order)    __define_initcall(ic_function, ".initcalls.smp.0100000" ic_order)
#define initcall_smp_ireq(ic_function, ic_order)    __define_initcall(ic_function, ".initcalls.smp.2100000" ic_order)
#define initcall_smp_syscall(ic_function, ic_order) __define_initcall(ic_function, ".initcalls.smp.2200000" ic_order)
#define initcall_smp_timer(ic_function, ic_order)   __define_initcall(ic_function, ".initcalls.smp.2300000" ic_order)
#define initcall_smp_late(ic_function, ic_order)    __define_initcall(ic_function, ".initcalls.smp.ff00000" ic_order)

extern const struct initcall initcalls_bsp_start[];
extern const struct initcall initcalls_bsp_end[];

extern const struct initcall initcalls_smp_start[];
extern const struct initcall initcalls_smp_end[];

void invoke_initcalls(const struct initcall* restrict start, const struct initcall* restrict end);

#endif /* C02B5460_568D_48F8_B826_99E2AE03A5AC */
