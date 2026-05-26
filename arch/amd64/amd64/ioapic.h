// SPDX-License-Identifier: BSD-3-Clause
#ifndef B4E1A2C3_7F8D_4E9B_A1B2_C3D4E5F60718
#define B4E1A2C3_7F8D_4E9B_A1B2_C3D4E5F60718
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Returns the IOAPIC GSI and interrupt characteristics for an ISA IRQ.
// Applies any ACPI interrupt source overrides from the MADT.
// Default (no override): gsi = irq, edge-triggered, active-high.
// NOTE: this is to be removed, HPET is preferred and is going to be
// used as a primary clocksource on amd64, the rest of legacy
// ISA devices should technically be accessible and iterable via ACPI
extern void isa_irq_route(uint8_t irq, uint32_t* out_gsi, bool* out_level, bool* out_active_low);

#endif /* B4E1A2C3_7F8D_4E9B_A1B2_C3D4E5F60718 */
