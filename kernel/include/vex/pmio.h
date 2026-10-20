#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

typedef struct pmio_region_private_s pmio_region_private_t;
typedef struct pmio_region_s pmio_region_t;

struct pmio_region_private_s {
    void* ctxdata;
};

struct pmio_region_s {
    pmio_region_private_t priv;
    uintptr_t base;
    size_t length;
};

void pmio_region_setup(pmio_region_t* restrict region);

uint8_t pmio_read8(const pmio_region_t* restrict pmio, uintptr_t offset);
uint16_t pmio_read16(const pmio_region_t* restrict pmio, uintptr_t offset);
uint32_t pmio_read32(const pmio_region_t* restrict pmio, uintptr_t offset);

void pmio_write8(const pmio_region_t* restrict pmio, uintptr_t offset, uint8_t value);
void pmio_write16(const pmio_region_t* restrict pmio, uintptr_t offset, uint16_t value);
void pmio_write32(const pmio_region_t* restrict pmio, uintptr_t offset, uint32_t value);

void pmio_read8s(const pmio_region_t* restrict pmio, uintptr_t offset, uint8_t* restrict data, size_t count);
void pmio_read16s(const pmio_region_t* restrict pmio, uintptr_t offset, uint16_t* restrict data, size_t count);
void pmio_read32s(const pmio_region_t* restrict pmio, uintptr_t offset, uint32_t* restrict data, size_t count);

void pmio_write8s(const pmio_region_t* restrict pmio, uintptr_t offset, const uint8_t* restrict data, size_t count);
void pmio_write16s(const pmio_region_t* restrict pmio, uintptr_t offset, const uint16_t* restrict data, size_t count);
void pmio_write32s(const pmio_region_t* restrict pmio, uintptr_t offset, const uint32_t* restrict data, size_t count);
