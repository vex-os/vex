#include <arch/amd64.pmio.h>
#include <vex/pmio.h>

void pmio_region_setup(pmio_region_t* restrict region)
{
    region->priv.ctxdata = nullptr;
}

uint8_t pmio_read8(const pmio_region_t* restrict pmio, uintptr_t offset)
{
    return port_read8(pmio->base + offset);
}

uint16_t pmio_read16(const pmio_region_t* restrict pmio, uintptr_t offset)
{
    return port_read16(pmio->base + offset);
}

uint32_t pmio_read32(const pmio_region_t* restrict pmio, uintptr_t offset)
{
    return port_read32(pmio->base + offset);
}

void pmio_write8(const pmio_region_t* restrict pmio, uintptr_t offset, uint8_t value)
{
    port_write8(pmio->base + offset, value);
}

void pmio_write16(const pmio_region_t* restrict pmio, uintptr_t offset, uint16_t value)
{
    port_write16(pmio->base + offset, value);
}

void pmio_write32(const pmio_region_t* restrict pmio, uintptr_t offset, uint32_t value)
{
    port_write32(pmio->base + offset, value);
}

void pmio_read8s(const pmio_region_t* restrict pmio, uintptr_t offset, uint8_t* restrict data, size_t count)
{
    port_read8s(pmio->base + offset, data, count);
}

void pmio_read16s(const pmio_region_t* restrict pmio, uintptr_t offset, uint16_t* restrict data, size_t count)
{
    port_read16s(pmio->base + offset, data, count);
}

void pmio_read32s(const pmio_region_t* restrict pmio, uintptr_t offset, uint32_t* restrict data, size_t count)
{
    port_read32s(pmio->base + offset, data, count);
}

void pmio_write8s(const pmio_region_t* restrict pmio, uintptr_t offset, const uint8_t* restrict data, size_t count)
{
    port_write8s(pmio->base + offset, data, count);
}

void pmio_write16s(const pmio_region_t* restrict pmio, uintptr_t offset, const uint16_t* restrict data, size_t count)
{
    port_write16s(pmio->base + offset, data, count);
}

void pmio_write32s(const pmio_region_t* restrict pmio, uintptr_t offset, const uint32_t* restrict data, size_t count)
{
    port_write32s(pmio->base + offset, data, count);
}
