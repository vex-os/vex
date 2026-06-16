#include <arch/amd64.pmio.h>
#include <vex/pmio.h>

void pmio_region_setup(struct pmio_region* restrict region)
{
    region->priv.ctxdata = nullptr;
}

uint8_t pmio_read8(const struct pmio_region* restrict pmio, uintptr_t offset)
{
    return pmread8(pmio->base + offset);
}

uint16_t pmio_read16(const struct pmio_region* restrict pmio, uintptr_t offset)
{
    return pmread16(pmio->base + offset);
}

uint32_t pmio_read32(const struct pmio_region* restrict pmio, uintptr_t offset)
{
    return pmread32(pmio->base + offset);
}

void pmio_write8(const struct pmio_region* restrict pmio, uintptr_t offset, uint8_t value)
{
    pmwrite8(pmio->base + offset, value);
}

void pmio_write16(const struct pmio_region* restrict pmio, uintptr_t offset, uint16_t value)
{
    pmwrite16(pmio->base + offset, value);
}

void pmio_write32(const struct pmio_region* restrict pmio, uintptr_t offset, uint32_t value)
{
    pmwrite32(pmio->base + offset, value);
}

void pmio_read8s(const struct pmio_region* restrict pmio, uintptr_t offset, uint8_t* restrict data, size_t count)
{
    pmread8s(pmio->base + offset, data, count);
}

void pmio_read16s(const struct pmio_region* restrict pmio, uintptr_t offset, uint16_t* restrict data, size_t count)
{
    pmread16s(pmio->base + offset, data, count);
}

void pmio_read32s(const struct pmio_region* restrict pmio, uintptr_t offset, uint32_t* restrict data, size_t count)
{
    pmread32s(pmio->base + offset, data, count);
}

void pmio_write8s(const struct pmio_region* restrict pmio, uintptr_t offset, const uint8_t* restrict data, size_t count)
{
    pmwrite8s(pmio->base + offset, data, count);
}

void pmio_write16s(const struct pmio_region* restrict pmio, uintptr_t offset, const uint16_t* restrict data, size_t count)
{
    pmwrite16s(pmio->base + offset, data, count);
}

void pmio_write32s(const struct pmio_region* restrict pmio, uintptr_t offset, const uint32_t* restrict data, size_t count)
{
    pmwrite32s(pmio->base + offset, data, count);
}
