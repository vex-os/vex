// SPDX-License-Identifier: BSD-3-Clause

#include <amd64/pmio.h>
#include <bus/isa.h>

uint8_t arch_isa_read8(struct isa_resource_pmio* restrict pmio, void* privdata, uintptr_t port)
{
    return pmread8(pmio->pmio + port);
}

uint16_t arch_isa_read16(struct isa_resource_pmio* restrict pmio, void* privdata, uintptr_t port)
{
    return pmread16(pmio->pmio + port);
}

void arch_isa_write8(struct isa_resource_pmio* restrict pmio, void* privdata, uintptr_t port, uint8_t value)
{
    pmwrite8(pmio->pmio + port, value);
}

void arch_isa_write16(struct isa_resource_pmio* restrict pmio, void* privdata, uintptr_t port, uint16_t value)
{
    pmwrite16(pmio->pmio + port, value);
}
