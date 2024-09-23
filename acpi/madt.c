/* SPDX-License-Identifier: BSD-2-Clause */
#include <acpi/madt.h>
#include <kern/panic.h>

const struct acpi_madt *madt;
const void *madt_entries;

const void *madt_iterate(const void *restrict entry)
{
    const struct madt_header *header = entry;
    const uint8_t *endptr = &((const uint8_t *)madt)[madt->header.length];
    const uint8_t *nextptr = &((const uint8_t *)entry)[header->length];

    if(nextptr >= endptr)
        return NULL;
    return nextptr;
}

void init_madt(void)
{
    if((madt = acpi_lookup("APIC")) == NULL) {
        panic("acpi: MADT table is not present");
        unreachable();
    }

    if(acpi_sdt_checksum(madt)) {
        panic("acpi: MADT failed checksum validation");
        unreachable();
    }

    /* Using index of one points at
     * space right after the very last
     * field defined in the structure */
    madt_entries = &madt[1];
}
