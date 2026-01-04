#ifndef _VEX_ACPI_H
#define _VEX_ACPI_H 1

#include <vex/compiler.h>

#include <stddef.h>
#include <stdint.h>

struct acpi_sdt_header {
    uint8_t signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __packed;

extern uint8_t acpi_generic_checksum(const void* restrict buffer, size_t sz);
extern uint8_t acpi_table_checksum(const void* restrict table);
extern const void* acpi_lookup(const char* restrict signature);

extern void init_acpi(void);

#endif
