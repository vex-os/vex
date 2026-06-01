#include <amd64/pmio.h>
#include <core/pmio.h>

uint8_t arch_pmio_read8(const struct pmio_region* restrict region, void* ctxdata, uintptr_t offset)
{
    return pmread8(region->base + offset);
}

uint16_t arch_pmio_read16(const struct pmio_region* restrict region, void* ctxdata, uintptr_t offset)
{
    return pmread16(region->base + offset);
}

uint32_t arch_pmio_read32(const struct pmio_region* restrict region, void* ctxdata, uintptr_t offset)
{
    return pmread32(region->base + offset);
}

void arch_pmio_write8(const struct pmio_region* restrict region, void* ctxdata, uintptr_t offset, uint8_t value)
{
    pmwrite8(region->base + offset, value);
}

void arch_pmio_write16(const struct pmio_region* restrict region, void* ctxdata, uintptr_t offset, uint16_t value)
{
    pmwrite16(region->base + offset, value);
}

void arch_pmio_write32(const struct pmio_region* restrict region, void* ctxdata, uintptr_t offset, uint32_t value)
{
    pmwrite32(region->base + offset, value);
}
