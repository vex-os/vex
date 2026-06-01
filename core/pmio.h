#ifndef A4384EF0_FCFC_42A0_904E_600EC97A9A7C
#define A4384EF0_FCFC_42A0_904E_600EC97A9A7C
#pragma once

#include <core/cdefs.h>
#include <stddef.h>
#include <stdint.h>

struct pmio_region {
    uintptr_t base;
    size_t length;
};

extern uint8_t arch_pmio_read8(const struct pmio_region* restrict region, void* ctxdata, uintptr_t offset);
extern uint16_t arch_pmio_read16(const struct pmio_region* restrict region, void* ctxdata, uintptr_t offset);
extern uint32_t arch_pmio_read32(const struct pmio_region* restrict region, void* ctxdata, uintptr_t offset);
extern void arch_pmio_write8(const struct pmio_region* restrict region, void* ctxdata, uintptr_t offset, uint8_t value);
extern void arch_pmio_write16(const struct pmio_region* restrict region, void* ctxdata, uintptr_t offset, uint16_t value);
extern void arch_pmio_write32(const struct pmio_region* restrict region, void* ctxdata, uintptr_t offset, uint32_t value);

#endif /* A4384EF0_FCFC_42A0_904E_600EC97A9A7C */
