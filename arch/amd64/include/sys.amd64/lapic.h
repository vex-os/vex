#ifndef DB23EE75_B5FE_49DD_985F_BC1F71B976EB
#define DB23EE75_B5FE_49DD_985F_BC1F71B976EB
#pragma once

#include <sys/cdefs.h>
#include <sys/hhdm.h>
#include <sys/types.h>

#define LAPIC_REG_ID     0x008
#define LAPIC_REG_EOI    0x02C
#define LAPIC_REG_SVR    0x03C
#define LAPIC_REG_ICR_LO 0x0C0
#define LAPIC_REG_ICR_HI 0x0C4

uint32_t lapic_read(uint32_t reg);
void lapic_write(uint32_t reg, uint32_t value);
void lapic_broadcast_ipi(uint8_t vector);

static __always_inline __nodiscard uint32_t lapic_query_id(void)
{
    return lapic_read(LAPIC_REG_ID) >> 24;
}

static __always_inline void lapic_send_eoi(void)
{
    lapic_write(LAPIC_REG_EOI, 0);
}

#endif /* DB23EE75_B5FE_49DD_985F_BC1F71B976EB */
