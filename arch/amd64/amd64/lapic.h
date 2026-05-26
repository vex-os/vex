// SPDX-License-Identifier: BSD-3-Clause
#ifndef DB23EE75_B5FE_49DD_985F_BC1F71B976EB
#define DB23EE75_B5FE_49DD_985F_BC1F71B976EB
#pragma once

#include <core/cdefs.h>
#include <mm/hhdm.h>
#include <stdint.h>

#define LAPIC_REG_ID        0x008
#define LAPIC_REG_EOI       0x02C
#define LAPIC_REG_SVR       0x03C
#define LAPIC_REG_ICR_LO    0x0C0
#define LAPIC_REG_ICR_HI    0x0C4
#define LAPIC_REG_TIMER_LVT 0x0C8
#define LAPIC_REG_TIMER_ICR 0x0E0
#define LAPIC_REG_TIMER_CCR 0x0E4
#define LAPIC_REG_TIMER_DCR 0x0F8

extern uint32_t lapic_read(uint32_t reg);
extern void lapic_write(uint32_t reg, uint32_t value);

#endif /* DB23EE75_B5FE_49DD_985F_BC1F71B976EB */
