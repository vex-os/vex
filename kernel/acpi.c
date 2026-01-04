#include <vex/acpi.h>
#include <vex/hhdm.h>
#include <vex/kprintf.h>
#include <vex/limine.h>
#include <vex/panic.h>
#include <vex/rsdp.h>
#include <vex/string.h>

static struct acpi_rsdp* rsdp = NULL;
static struct acpi_xsdp* xsdp = NULL;
static struct acpi_sdt_header* sdt_root = NULL;

static volatile __limine_request struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0,
};

static __always_inline __nodiscard inline size_t calc_sdt_count(void)
{
    size_t rsize = sdt_root->length - sizeof(struct acpi_sdt_header);
    if(rsdp->revision == 0U)
        return rsize / sizeof(uint32_t);
    return rsize / sizeof(uint64_t);
}

static __always_inline __nodiscard inline uintptr_t get_sdt_at(size_t index)
{
    if(rsdp->revision == 0U)
        return ((const uint32_t*)(&sdt_root[1]))[index];
    return ((const uint64_t*)(&sdt_root[1]))[index];
}

uint8_t acpi_generic_checksum(const void* restrict buffer, size_t sz)
{
    size_t i;
    uint8_t result = 0U;
    const uint8_t* psz = buffer;

    for(i = 0U; i < sz; result += psz[i++]) {
        // empty
    }

    return result;
}

uint8_t acpi_table_checksum(const void* restrict table)
{
    const struct acpi_sdt_header* sdt = table;
    return acpi_generic_checksum(table, sdt->length);
}

const void* acpi_lookup(const char* restrict signature)
{
    size_t i;
    size_t count = calc_sdt_count();
    const struct acpi_sdt_header* sdt;

    for(i = 0U; i < count; ++i) {
        sdt = phys_to_hhdm(get_sdt_at(i));

        if(0 == memcmp(sdt->signature, signature, sizeof(sdt->signature)))
            return sdt;
        continue;
    }

    return NULL;
}

void init_acpi(void)
{
    uint8_t checksum;

    if(rsdp_request.response == NULL) {
        panic("acpi: limine_rsdp_request has no response");
        unreachable();
    }

    rsdp = rsdp_request.response->address;
    xsdp = rsdp_request.response->address;

    if(rsdp->revision < 2) {
        checksum = acpi_generic_checksum(rsdp, sizeof(struct acpi_rsdp));
    }
    else {
        checksum = acpi_generic_checksum(xsdp, xsdp->length);
    }

    if(checksum) {
        panic("acpi: rsdp/xsdp failed checksum validation");
        unreachable();
    }

    if(rsdp->revision < 2) {
        sdt_root = phys_to_hhdm(rsdp->rsdt);
    }
    else {
        sdt_root = phys_to_hhdm(xsdp->xsdt);
    }

    if(acpi_table_checksum(sdt_root)) {
        panic("acpi: rsdt/xsdt failed checksum validation");
        unreachable();
    }

    size_t i;
    size_t count = calc_sdt_count();
    const struct acpi_sdt_header* sdt;

    for(i = 0U; i < count; ++i) {
        sdt = phys_to_hhdm(get_sdt_at(i));

        kprintf(KP_INFORM, "acpi: table[%zu]: %p: %.4s", i, (const void*)(sdt), sdt->signature);
    }
}
