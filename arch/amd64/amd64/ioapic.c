#include <core/kprintf.h>
#include <core/panic.h>
#include <init/initcall.h>
#include <ireq/intctl.h>
#include <lib/sprintf.h>
#include <lib/string.h>
#include <mm/hhdm.h>
#include <mm/malloc.h>
#include <mm/vmm.h>
#include <mm/vprot.h>
#include <uacpi/acpi.h>
#include <uacpi/tables.h>

#define IOAPIC_MMIO_SELECT 0 ///< IOAPIC REGSEL (offset 0x00)
#define IOAPIC_MMIO_WINDOW 4 ///< IOAPIC IOWIN  (offset 0x10)

#define IOAPIC_REG_ID         UINT32_C(0x00000000) ///< Bits 24-27: APIC ID for this I/O APIC
#define IOAPIC_REG_VER        UINT32_C(0x00000001) ///< Bits 16-23: Maximum redirection entry, Bits 0-7: APIC version
#define IOAPIC_REG_TBL_BASE   UINT32_C(0x00000010) ///< Redirection table base
#define IOAPIC_REG_TBL_STRIDE UINT32_C(0x00000002) ///< Redirection table stride

#define RTE_DELIVERY_FIXED  0 ///< fixed
#define RTE_DELIVERY_LOWEST 1 ///< lowest priority
#define RTE_DELIVERY_SMI    2 ///< SMI
#define RTE_DELIVERY_NMI    4 ///< NMI
#define RTE_DELIVERY_INIT   5 ///< INIT
#define RTE_DELIVERY_EXTINT 7 ///< ExtINT

struct rte_entry {
    uint64_t vector : 8;        ///< Interrupt vector number
    uint64_t delivery_mode : 3; ///< Delivery mode (RTE_DELIVERY constants)
    uint64_t dest_logical : 1;  ///< If set, destination is a logical APIC ID
    uint64_t delivery_stat : 1; ///< Read-only; if set, the APIC is still sending the interrupt
    uint64_t active_low : 1;    ///< If set, the interrupt is active low; otherwise, active high
    uint64_t remote_irr : 1;    ///< Read-only; if set, the APIC has received an interrupt and is waiting for an EOI
    uint64_t level : 1;         ///< If set, the interrupt is level-triggered; otherwise, edge-triggered
    uint64_t masked : 1;        ///< If set, the interrupt is masked aka disabled
    uint64_t reserved : 39;     ///< Reserved for whatever reasons, I'm not kink-shaming anyone
    uint64_t dest : 8;          ///< Destination APIC ID
} __packed;

static_assert(8 == sizeof(struct rte_entry));

#define IOAPIC_VER_MAXREDIR_SHIFT 16
#define IOAPIC_VER_MAXREDIR_MASK  0xFF

struct ioapic_data {
    volatile uint32_t* base;
};

static uint32_t ioapic_read(struct ioapic_data* data, uint32_t index)
{
    data->base[IOAPIC_MMIO_SELECT] = index;

    return data->base[IOAPIC_MMIO_WINDOW];
}

static void ioapic_write(struct ioapic_data* data, uint32_t index, uint32_t val)
{
    data->base[IOAPIC_MMIO_SELECT] = index;
    data->base[IOAPIC_MMIO_WINDOW] = val;
}

static struct rte_entry ioapic_read_rte(struct ioapic_data* data, uint8_t irq)
{
    uint32_t index = IOAPIC_REG_TBL_BASE + irq * IOAPIC_REG_TBL_STRIDE;

    uint64_t raw = UINT64_C(0);
    raw |= (uint64_t)(ioapic_read(data, index + 0));
    raw |= (uint64_t)(ioapic_read(data, index + 1)) << 32;

    struct rte_entry entry;
    memcpy(&entry, &raw, sizeof(entry));
    return entry;
}

static void ioapic_write_rte(struct ioapic_data* data, uint8_t irq, struct rte_entry entry)
{
    uint32_t index = IOAPIC_REG_TBL_BASE + irq * IOAPIC_REG_TBL_STRIDE;

    uint64_t raw;
    memcpy(&raw, &entry, sizeof(raw));

    ioapic_write(data, index + 0, (uint32_t)(raw));
    ioapic_write(data, index + 1, (uint32_t)(raw >> 32));
}

static void ioapic_op_mask(struct intctl* restrict controller, uint32_t gsi)
{
    struct ioapic_data* data = controller->ctxdata;

    uint8_t irq = (uint8_t)(gsi - controller->gsi_base);

    struct rte_entry entry = ioapic_read_rte(data, irq);
    entry.masked = 1;

    ioapic_write_rte(data, irq, entry);
}

static void ioapic_op_unmask(struct intctl* restrict controller, uint32_t gsi)
{
    struct ioapic_data* data = controller->ctxdata;

    uint8_t irq = (uint8_t)(gsi - controller->gsi_base);

    struct rte_entry entry = ioapic_read_rte(data, irq);
    entry.masked = 0;

    ioapic_write_rte(data, irq, entry);
}

static void ioapic_op_route(struct intctl* restrict controller, uint32_t gsi, uint8_t vector, uint32_t hart, unsigned trigger,
    unsigned polarity)
{
    struct ioapic_data* data = controller->ctxdata;

    uint8_t irq = (uint8_t)(gsi - controller->gsi_base);

    struct rte_entry entry = { 0 };
    entry.vector = vector;
    entry.dest = hart;
    entry.level = (trigger == TRIGGER_LEVEL);
    entry.active_low = (polarity == ACTIVE_LOW);
    entry.masked = 1;

    ioapic_write_rte(data, irq, entry);
}

static const struct intctl_ops ioapic_ops = {
    .mask = &ioapic_op_mask,
    .unmask = &ioapic_op_unmask,
    .route = &ioapic_op_route,
};

static uacpi_iteration_decision ioapic_madt_callback(uacpi_handle user, struct acpi_entry_hdr* entry)
{
    struct acpi_madt_ioapic* ioapic;
    struct ioapic_data* data;
    struct intctl* controller;

    void* virt;
    uint32_t id;
    uint32_t ver;
    uint8_t i;

    if(entry->type == ACPI_MADT_ENTRY_TYPE_IOAPIC) {
        ioapic = (struct acpi_madt_ioapic*)(entry);

        data = kmalloc(sizeof(struct ioapic_data));
        controller = kmalloc(sizeof(struct intctl));

        if(data == nullptr || controller == nullptr) {
            panic("ioapic: out of memory");
        }

        virt = phys_to_virt(ioapic->address);

        if(vmm_map(&sys_pagemap, (uintptr_t)(virt), ioapic->address, VPROT_RW | VPROT_NOCACHE)) {
            panic("ioapic: failed to map mmio at %p", (const void*)((uintptr_t)(ioapic->address)));
        }

        data->base = (volatile uint32_t*)(virt);

        id = ioapic_read(data, IOAPIC_REG_ID) >> 24;
        ver = ioapic_read(data, IOAPIC_REG_VER);

        controller->ops = &ioapic_ops;
        controller->gsi_base = ioapic->gsi_base;
        controller->gsi_count = ((ver >> IOAPIC_VER_MAXREDIR_SHIFT) & IOAPIC_VER_MAXREDIR_MASK) + 1;
        controller->ctxdata = data;

        snprintf(controller->identifier, sizeof(controller->identifier), "ioapic-%u", (unsigned)(id));

        for(i = 0; i < controller->gsi_count; ++i) {
            ioapic_write_rte(data, i, (struct rte_entry) { .masked = 1 });
        }

        if(intctl_register(controller)) {
            panic("ioapic: failed to register interrupt controller");
        }
    }

    return UACPI_ITERATION_DECISION_CONTINUE;
}

define_initcall(amd64_ioapic, INITCALL_STAGE_BSP_IREQ, INITCALL_ORDER_FIRST)
{
    uacpi_table madt;
    uacpi_status status;

    status = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &madt);

    if(uacpi_unlikely_error(status)) {
        panic("ioapic: acpi madt not found: %s", uacpi_status_to_string(status));
    }

    uacpi_for_each_subtable(madt.hdr, sizeof(struct acpi_madt), &ioapic_madt_callback, nullptr);
    uacpi_table_unref(&madt);
}
