#include <vex/madt.h>
#include <vex/panic.h>

const struct acpi_madt* madt = NULL;
const void* madt_entries = NULL;

const void* madt_next_entry(const void* restrict current)
{
    const struct madt_header* header = current;
    const uint8_t* endptr = &((const uint8_t*)(madt))[madt->header.length];
    const uint8_t* nextptr = &((const uint8_t*)current)[header->length];

    if(nextptr >= endptr)
        return NULL;
    return nextptr;
}

void init_madt(void)
{
    madt = acpi_lookup("APIC");

    if(madt == NULL) {
        panic("madt: madt table is not present");
        unreachable();
    }

    if(acpi_table_checksum(madt)) {
        panic("madt: madt table failed checksum validation");
        unreachable();
    }

    // Using index of one points at the space
    // right after the very last field defined
    // in the structure; this has much less
    //  visual noise than pointer casts
    madt_entries = &madt[1];
}
