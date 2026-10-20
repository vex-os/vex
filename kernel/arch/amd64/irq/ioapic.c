#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/intctl.h>
#include <vex/kmalloc.h>
#include <vex/mmio.h>
#include <vex/panic.h>
#include <vex/sprintf.h>
#include <vex/string.h>
#include <vex/vprot.h>

#define IOAPIC_MMIO_SELECT 0
#define IOAPIC_MMIO_WINDOW 4
#define IOAPIC_MMIO_SIZE   0x20

#define IOAPIC_REG_ID         UINT32_C(0x00000000)
#define IOAPIC_REG_VER        UINT32_C(0x00000001)
#define IOAPIC_REG_TBL_BASE   UINT32_C(0x00000010)
#define IOAPIC_REG_TBL_STRIDE UINT32_C(0x00000002)

#define RTE_DELIVERY_FIXED  0
#define RTE_DELIVERY_LOWEST 1
#define RTE_DELIVERY_SMI    2
#define RTE_DELIVERY_NMI    4
#define RTE_DELIVERY_INIT   5
#define RTE_DELIVERY_EXTINT 7

#define IOAPIC_VER_MAXREDIR_SHIFT 16
#define IOAPIC_VER_MAXREDIR_MASK  0xFF

typedef struct rte_entry_s rte_entry_t;
typedef struct ioapic_data_s ioapic_data_t;

struct rte_entry_s {
    uint64_t vector : 8;
    uint64_t delivery_mode : 3;
    uint64_t dest_logical : 1;
    uint64_t delivery_stat : 1;
    uint64_t active_low : 1;
    uint64_t remote_irr : 1;
    uint64_t level : 1;
    uint64_t masked : 1;
    uint64_t reserved : 39;
    uint64_t dest : 8;
} __packed;

static_assert(8 == sizeof(rte_entry_t));

struct ioapic_data_s {
    volatile uint32_t* base;
};

static uint32_t ioapic_read(ioapic_data_t* data, uint32_t index)
{
    data->base[IOAPIC_MMIO_SELECT] = index;

    return data->base[IOAPIC_MMIO_WINDOW];
}

static void ioapic_write(ioapic_data_t* data, uint32_t index, uint32_t val)
{
    data->base[IOAPIC_MMIO_SELECT] = index;
    data->base[IOAPIC_MMIO_WINDOW] = val;
}

static rte_entry_t ioapic_get_rte(ioapic_data_t* data, uint8_t irq)
{
    uint32_t index = IOAPIC_REG_TBL_BASE + irq * IOAPIC_REG_TBL_STRIDE;

    uint64_t raw = UINT64_C(0);
    raw |= (uint64_t)ioapic_read(data, index + 0);
    raw |= (uint64_t)ioapic_read(data, index + 1) << 32;

    rte_entry_t entry;
    memcpy(&entry, &raw, sizeof(entry));
    return entry;
}

static void ioapic_set_rte(ioapic_data_t* data, uint8_t irq, rte_entry_t entry)
{
    uint64_t raw;
    memcpy(&raw, &entry, sizeof(raw));

    uint32_t index = IOAPIC_REG_TBL_BASE + irq * IOAPIC_REG_TBL_STRIDE;
    ioapic_write(data, index + 0, raw);
    ioapic_write(data, index + 1, raw >> 32);
}

static void ioapic_mask(interrupt_controller_t* restrict controller, uint32_t gsi)
{
    ioapic_data_t* data = controller->ctxdata;

    uint8_t irq = gsi - controller->gsi_base;

    rte_entry_t entry = ioapic_get_rte(data, irq);
    entry.masked = 1;

    ioapic_set_rte(data, irq, entry);
}

static void ioapic_unmask(interrupt_controller_t* restrict controller, uint32_t gsi)
{
    ioapic_data_t* data = controller->ctxdata;

    uint8_t irq = gsi - controller->gsi_base;

    rte_entry_t entry = ioapic_get_rte(data, irq);
    entry.masked = 0;

    ioapic_set_rte(data, irq, entry);
}

static void ioapic_route(interrupt_controller_t* restrict controller, uint32_t gsi, uint8_t vector, uint32_t hart, unsigned trigger,
    unsigned polarity)
{
    ioapic_data_t* data = controller->ctxdata;

    rte_entry_t entry = {};
    entry.vector = vector;
    entry.dest = hart;
    entry.level = (trigger == TRIGGER_LEVEL);
    entry.active_low = (polarity == ACTIVE_LOW);
    entry.masked = 1;

    ioapic_set_rte(data, gsi - controller->gsi_base, entry);
}

static const interrupt_controller_ops_t ioapic_ops = {
    .mask = &ioapic_mask,
    .unmask = &ioapic_unmask,
    .route = &ioapic_route,
};

static uacpi_iteration_decision ioapic_madt_callback(uacpi_handle user, struct acpi_entry_hdr* entry)
{
    if(entry->type == ACPI_MADT_ENTRY_TYPE_IOAPIC) {
        struct acpi_madt_ioapic* ioapic = (struct acpi_madt_ioapic*)(entry);

        ioapic_data_t* data = kmalloc(sizeof(ioapic_data_t));
        interrupt_controller_t* controller = kmalloc(sizeof(interrupt_controller_t));

        if(data == nullptr || controller == nullptr) {
            panic("ioapic: out of memory");
        }

        data->base = mmio_map(ioapic->address, IOAPIC_MMIO_SIZE, VPROT_MMIO);

        if(data->base == nullptr) {
            panic("ioapic: failed to map registers");
        }

        uint32_t id = ioapic_read(data, IOAPIC_REG_ID) >> 24;
        uint32_t ver = ioapic_read(data, IOAPIC_REG_VER);

        controller->ops = &ioapic_ops;
        controller->gsi_base = ioapic->gsi_base;
        controller->gsi_count = ((ver >> IOAPIC_VER_MAXREDIR_SHIFT) & IOAPIC_VER_MAXREDIR_MASK) + 1;
        controller->ctxdata = data;

        controller->name = kasprintf("ioapic%u", (unsigned)id);

        if(!controller->name) {
            panic("ioapic: out of memory");
        }

        rte_entry_t masked = {};
        masked.masked = 1;

        for(uint8_t i = 0; i < controller->gsi_count; ++i) {
            ioapic_set_rte(data, i, masked);
        }

        int retval = interrupt_controller_add(controller);

        if(retval < 0) {
            panic("ioapic: %s", strerror(retval));
        }
    }

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static void arch_init_ioapic(void)
{
    uacpi_table madt;
    uacpi_status status = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &madt);

    if(uacpi_unlikely_error(status)) {
        panic("ioapic: %s", uacpi_status_to_string(status));
    }

    uacpi_for_each_subtable(madt.hdr, sizeof(struct acpi_madt), &ioapic_madt_callback, nullptr);
    uacpi_table_unref(&madt);
}

initcall_bsp_irq(arch_init_ioapic, INIT_FIRST);
