#ifndef _VEX_RSDP_H
#define _VEX_RSDP_H 1

#include <vex/compiler.h>

#include <stdint.h>

struct acpi_rsdp {
    uint8_t signature[8];
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t revision;
    uint32_t rsdt;
} __packed;

struct acpi_xsdp {
    struct acpi_rsdp rsdp;
    uint32_t length;
    uint64_t xsdt;
    uint8_t checksum;
    uint8_t reserved[3];
} __packed;

#endif
