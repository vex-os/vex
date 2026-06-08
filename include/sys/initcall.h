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

#define __define_initcall(i_function, i_section) \
    static const __used __section(i_section)     \
    struct initcall __initcall_##i_function = {  \
        .function = &(i_function),               \
        .identifier = #i_function,               \
    }

#define initcall_bsp_early(function, order)     __define_initcall(function, ".initcalls.bsp.0000000" order)
#define initcall_bsp_arch(function, order)      __define_initcall(function, ".initcalls.bsp.0100000" order)
#define initcall_bsp_bootinfo(function, order)  __define_initcall(function, ".initcalls.bsp.0200000" order)
#define initcall_bsp_pmm(function, order)       __define_initcall(function, ".initcalls.bsp.1000000" order)
#define initcall_bsp_slab(function, order)      __define_initcall(function, ".initcalls.bsp.1100000" order)
#define initcall_bsp_vmm(function, order)       __define_initcall(function, ".initcalls.bsp.1200000" order)
#define initcall_bsp_heap(function, order)      __define_initcall(function, ".initcalls.bsp.1300000" order)
#define initcall_bsp_ireq(function, order)      __define_initcall(function, ".initcalls.bsp.2000000" order)
#define initcall_bsp_timer(function, order)     __define_initcall(function, ".initcalls.bsp.2100000" order)
#define initcall_bsp_multicore(function, order) __define_initcall(function, ".initcalls.bsp.2200000" order)
#define initcall_bsp_bus(function, order)       __define_initcall(function, ".initcalls.bsp.3000000" order)
#define initcall_bsp_vfs(function, order)       __define_initcall(function, ".initcalls.bsp.3100000" order)
#define initcall_bsp_dev(function, order)       __define_initcall(function, ".initcalls.bsp.3200000" order)
#define initcall_bsp_fs(function, order)        __define_initcall(function, ".initcalls.bsp.3300000" order)
#define initcall_bsp_cpudev(function, order)    __define_initcall(function, ".initcalls.bsp.3400000" order)
#define initcall_bsp_services(function, order)  __define_initcall(function, ".initcalls.bsp.4000000" order)
#define initcall_bsp_late(function, order)      __define_initcall(function, ".initcalls.bsp.ff00000" order)

#define initcall_smp_hart(function, order)  __define_initcall(function, ".initcalls.smp.0000000" order)
#define initcall_smp_arch(function, order)  __define_initcall(function, ".initcalls.smp.0100000" order)
#define initcall_smp_ireq(function, order)  __define_initcall(function, ".initcalls.smp.2100000" order)
#define initcall_smp_timer(function, order) __define_initcall(function, ".initcalls.smp.2200000" order)
#define initcall_smp_late(function, order)  __define_initcall(function, ".initcalls.smp.ff00000" order)

extern const struct initcall initcalls_bsp_start[];
extern const struct initcall initcalls_bsp_end[];

extern const struct initcall initcalls_smp_start[];
extern const struct initcall initcalls_smp_end[];

void invoke_initcalls(const struct initcall* restrict start, const struct initcall* restrict end);

#endif /* C02B5460_568D_48F8_B826_99E2AE03A5AC */
