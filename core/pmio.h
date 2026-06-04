#ifndef A4384EF0_FCFC_42A0_904E_600EC97A9A7C
#define A4384EF0_FCFC_42A0_904E_600EC97A9A7C
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

struct pmio_region_private {
    void* ctxdata;
};

struct pmio_region {
    struct pmio_region_private priv;
    uintptr_t base;
    size_t length;
};

void pmio_region_setup(struct pmio_region* restrict region);

uint8_t pmio_read8(const struct pmio_region* restrict pmio, uintptr_t offset);
uint16_t pmio_read16(const struct pmio_region* restrict pmio, uintptr_t offset);
uint32_t pmio_read32(const struct pmio_region* restrict pmio, uintptr_t offset);

void pmio_write8(const struct pmio_region* restrict pmio, uintptr_t offset, uint8_t value);
void pmio_write16(const struct pmio_region* restrict pmio, uintptr_t offset, uint16_t value);
void pmio_write32(const struct pmio_region* restrict pmio, uintptr_t offset, uint32_t value);

void pmio_read8_string(const struct pmio_region* restrict pmio, uintptr_t offset, uint8_t* restrict data, size_t count);
void pmio_read16_string(const struct pmio_region* restrict pmio, uintptr_t offset, uint16_t* restrict data, size_t count);
void pmio_read32_string(const struct pmio_region* restrict pmio, uintptr_t offset, uint32_t* restrict data, size_t count);

void pmio_write8_string(const struct pmio_region* restrict pmio, uintptr_t offset, const uint8_t* restrict data, size_t count);
void pmio_write16_string(const struct pmio_region* restrict pmio, uintptr_t offset, const uint16_t* restrict data, size_t count);
void pmio_write32_string(const struct pmio_region* restrict pmio, uintptr_t offset, const uint32_t* restrict data, size_t count);

#endif /* A4384EF0_FCFC_42A0_904E_600EC97A9A7C */
