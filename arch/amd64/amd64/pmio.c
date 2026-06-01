#include <amd64/pmio.h>
#include <core/pmio.h>

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

void pmio_read8_string(const struct pmio_region* restrict pmio, uintptr_t offset, uint8_t* restrict data, size_t count)
{
    pmread8_string(pmio->base + offset, data, count);
}

void pmio_read16_string(const struct pmio_region* restrict pmio, uintptr_t offset, uint16_t* restrict data, size_t count)
{
    pmread16_string(pmio->base + offset, data, count);
}

void pmio_read32_string(const struct pmio_region* restrict pmio, uintptr_t offset, uint32_t* restrict data, size_t count)
{
    pmread32_string(pmio->base + offset, data, count);
}

void pmio_write8_string(const struct pmio_region* restrict pmio, uintptr_t offset, const uint8_t* restrict data, size_t count)
{
    pmwrite8_string(pmio->base + offset, data, count);
}

void pmio_write16_string(const struct pmio_region* restrict pmio, uintptr_t offset, const uint16_t* restrict data, size_t count)
{
    pmwrite16_string(pmio->base + offset, data, count);
}

void pmio_write32_string(const struct pmio_region* restrict pmio, uintptr_t offset, const uint32_t* restrict data, size_t count)
{
    pmwrite32_string(pmio->base + offset, data, count);
}
