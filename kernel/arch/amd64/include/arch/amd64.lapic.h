#pragma once

#include <vex/compiler.h>
#include <vex/hhdm.h>
#include <vex/types.h>

#define LAPIC_REG_ID         0x008
#define LAPIC_REG_EOI        0x02C
#define LAPIC_REG_SVR        0x03C
#define LAPIC_REG_ICR_LO     0x0C0
#define LAPIC_REG_ICR_HI     0x0C4
#define LAPIC_REG_LVT_TIMER  0x0C8
#define LAPIC_REG_TMRINITCNT 0x0E0
#define LAPIC_REG_TMRCURRCNT 0x0E4
#define LAPIC_REG_TMRDIV     0x0F8

#define LAPIC_TIMER_ONESHOT  UINT32_C(0x00000000)
#define LAPIC_TIMER_PERIODIC UINT32_C(0x00020000)
#define LAPIC_TIMER_MASKED   UINT32_C(0x00010000)
#define LAPIC_TIMER_DIV1     UINT32_C(0x0000000B)

#define LAPIC_IPI_SPECIFIC  UINT32_C(0x00000000)
#define LAPIC_IPI_EVERYONE  UINT32_C(0x00084000)
#define LAPIC_IPI_EXCEPT_ME UINT32_C(0x000C4000)

uint32_t lapic_read(uint32_t reg);
void lapic_write(uint32_t reg, uint32_t value);
void lapic_ipi(uint32_t target, uint32_t mode, uint8_t vector);

static __always_inline __nodiscard uint32_t lapic_query_id(void)
{
    return lapic_read(LAPIC_REG_ID) >> 24;
}

static __always_inline void lapic_send_eoi(void)
{
    lapic_write(LAPIC_REG_EOI, 0);
}
