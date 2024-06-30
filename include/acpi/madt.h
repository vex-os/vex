// SPDX-License-Identifier: Zlib
#ifndef INCLUDE_ACPI_MADT_H
#define INCLUDE_ACPI_MADT_H
#include <acpi/acpi.h>
#include <stddef.h>

#define MADT_PCAT_COMPAT            UINT32_C(0x00000001)

#define MADT_LOCAL_APIC             UINT8_C(0x00)
#define MADT_IOAPIC                 UINT8_C(0x01)
#define MADT_IOAPIC_IRQ_OVERRIDE    UINT8_C(0x02)
#define MADT_IOAPIC_NMI_SOURCE      UINT8_C(0x03)
#define MADT_LOCAL_APIC_NMI         UINT8_C(0x04)
#define MADT_LOCAL_APIC_OVERRIDE    UINT8_C(0x05)

#define MADT_LOCAL_APIC_ENABLED     UINT32_C(0x00000001)
#define MADT_LOCAL_APIC_ONLINE_CAP  UINT32_C(0x00000002)

#define MADT_INTI_ACTIVE_HIGH       UINT16_C(0x0001)
#define MADT_INTI_ACTIVE_LOW        UINT16_C(0x0003)
#define MADT_INTI_TRIGGER_EDGE      UINT16_C(0x0010)
#define MADT_INTI_TRIGGER_LEVEL     UINT16_C(0x0030)

struct acpi_madt {
    struct acpi_sdt_header header;
    uint32_t address;
    uint32_t flags;
} __packed;

struct madt_header {
    uint8_t type;
    uint8_t length;
} __packed;

struct madt_local_apic {
    struct madt_header header;
    uint8_t processor_id;
    uint8_t controller_id;
    uint32_t flags;
} __packed;

struct madt_ioapic {
    struct madt_header header;
    uint8_t controller_id;
    uint8_t reserved;
    uint32_t address;
    uint32_t gsi_base;
} __packed;

struct madt_ioapic_irq_override {
    struct madt_header header;
    uint8_t bus_type;
    uint8_t source;
    uint32_t gsi_vector;
    uint16_t inti_flags;
} __packed;

struct madt_ioapic_nmi_source {
    struct madt_header header;
    uint16_t inti_flags;
    uint32_t gsi_vector;
} __packed;

struct madt_local_apic_nmi {
    struct madt_header header;
    uint8_t processor_id;
    uint16_t inti_flags;
    uint8_t lint;
} __packed;

struct madt_local_apic_override {
    struct madt_header header;
    uint16_t reserved;
    uint32_t address;
} __packed;

extern const struct acpi_madt *madt;
extern const void *madt_entries;

// Helper function to minimize boilerplate
// code when looping through MADT sub-tables.
const void *madt_iterate(const void *restrict entry);

void init_madt(void);

#endif // INCLUDE_ACPI_MADT_H
