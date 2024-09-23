/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef INCLUDE_ACPI_ACPI_H
#define INCLUDE_ACPI_ACPI_H
#include <kern/compiler.h>
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

uint8_t acpi_generic_checksum(const void *restrict buf, size_t sz);
uint8_t acpi_sdt_checksum(const void *restrict table);
const void *acpi_lookup(const char *restrict signature);

void init_acpi(void);

#endif /* INCLUDE_ACPI_ACPI_H */
