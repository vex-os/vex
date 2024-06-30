// SPDX-License-Identifier: Zlib
#include <acpi/acpi.h>
#include <acpi/rsdp.h>
#include <kern/panic.h>
#include <kern/printf.h>
#include <limine.h>
#include <mm/hhdm.h>
#include <string.h>

static struct acpi_rsdp *rsdp;
static struct acpi_xsdp *xsdp;
static struct acpi_sdt_header *sdt_root;

static volatile struct limine_rsdp_request __used request = {
    .id = LIMINE_RSDP_REQUEST,
    .response = NULL,
    .revision = 0,
};

static __always_inline __nodiscard inline size_t get_sdt_count(void)
{
    if(rsdp->revision == 0)
        return (sdt_root->length - sizeof(struct acpi_sdt_header)) / sizeof(uint32_t);
    return (sdt_root->length - sizeof(struct acpi_sdt_header)) / sizeof(uint64_t);
}

static __always_inline __nodiscard inline uintptr_t get_sdt_at(size_t index)
{
    if(rsdp->revision == 0)
        return ((const uint32_t *)(&sdt_root[1]))[index];
    return ((const uint64_t *)(&sdt_root[1]))[index];
}

uint8_t acpi_generic_checksum(const void *restrict buf, size_t sz)
{
    size_t i;
    uint8_t result = 0x00;
    const uint8_t *psz = buf;
    for(i = 0; i < sz; result += psz[i++]);
    return result;
}

uint8_t acpi_sdt_checksum(const void *restrict table)
{
    const struct acpi_sdt_header *sdt = table;
    return acpi_generic_checksum(table, sdt->length);
}

const void *acpi_lookup(const char *restrict signature)
{
    size_t i;
    size_t count = get_sdt_count();
    const struct acpi_sdt_header *sdt;

    for(i = 0; i < count; ++i) {
        sdt = phys_to_hhdm(get_sdt_at(i));
        if(memcmp(sdt->signature, signature, sizeof(sdt->signature)))
            continue;
        return sdt;
    }

    return NULL;
}

void init_acpi(void)
{
    if(!request.response) {
        panic("acpi: limine_rsdp_request has no response");
        unreachable();
    }

    rsdp = request.response->address;
    xsdp = request.response->address;

    if(acpi_generic_checksum(rsdp, sizeof(struct acpi_rsdp))) {
        panic("acpi: RSDP checksum validation fail");
        unreachable();
    }

    if(rsdp->revision == 0)
        sdt_root = phys_to_hhdm(rsdp->rsdt);
    else sdt_root = phys_to_hhdm(xsdp->xsdt);

    if(acpi_sdt_checksum(sdt_root)) {
        panic("acpi: RSDT/XSDT checksum validation fail");
        unreachable();
    }
}
