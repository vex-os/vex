// SPDX-License-Identifier: BSD-3-Clause
#ifndef A1B2C3D4_E5F6_7890_ABCD_EF1234567890
#define A1B2C3D4_E5F6_7890_ABCD_EF1234567890
#pragma once

#include <stdint.h>

// Called once per I/O APIC entry found in ACPI MADT
extern void ioapic_register(uintptr_t phys, uint32_t gsi_base);

#endif /* A1B2C3D4_E5F6_7890_ABCD_EF1234567890 */
