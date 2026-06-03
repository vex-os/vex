#include <arch/hart.h>
#include <bus/pci.h>
#include <core/assert.h>
#include <core/endian.h>
#include <core/kprintf.h>
#include <dev/ata.h>
#include <dev/ata_atapi.h>
#include <dev/blk_device.h>
#include <dev/blk_event.h>
#include <dev/blk_name.h>
#include <init/initcall.h>
#include <lib/sprintf.h>
#include <lib/string.h>
#include <mm/hhdm.h>
#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/pdma.h>

#define HBA_CAP 0x00 ///< Host Capabilities
#define HBA_GHC 0x04 ///< Global Host Control
#define HBA_IS  0x08 ///< Interrupt Status (global)
#define HBA_PI  0x0C ///< Ports Implemented

#define HBA_GHC_AE UINT32_C(0x80000000) ///< AHCI Enable
#define HBA_GHC_IE UINT32_C(0x00000002) ///< Interrupt Enable

#define HBA_PORT_BASE 0x100 ///< Offset of port 0 registers
#define HBA_PORT_SIZE 0x80  ///< Per-port register stride

#define PORT_REG_CLB  0x00 ///< Command List Base Address
#define PORT_REG_CLBU 0x04 ///< Command List Base Address Upper 32 bits
#define PORT_REG_FB   0x08 ///< FIS Base Address
#define PORT_REG_FBU  0x0C ///< FIS Base Address Upper 32 bits
#define PORT_REG_IS   0x10 ///< Interrupt Status
#define PORT_REG_IE   0x14 ///< Interrupt Enable
#define PORT_REG_CMD  0x18 ///< Command and Status
#define PORT_REG_TFD  0x20 ///< Task File Data
#define PORT_REG_SIG  0x24 ///< Signature
#define PORT_REG_SSTS 0x28 ///< SATA Status (SCR0)
#define PORT_REG_SERR 0x30 ///< SATA Error (SCR1)
#define PORT_REG_SACT 0x34 ///< SATA Active
#define PORT_REG_CI   0x38 ///< Command Issue

#define PORT_CMD_ST  UINT32_C(0x00000001) ///< Start
#define PORT_CMD_FRE UINT32_C(0x00000010) ///< FIS Receive Enable
#define PORT_CMD_FR  UINT32_C(0x00004000) ///< FIS Receive Running
#define PORT_CMD_CR  UINT32_C(0x00008000) ///< Command List Running

#define PORT_TFD_ERR UINT32_C(0x00000001) ///< Task File Error
#define PORT_TFD_DRQ UINT32_C(0x00000008) ///< Data Request
#define PORT_TFD_BSY UINT32_C(0x00000080) ///< Busy

#define PORT_SSTS_DET_PRESENT 3 ///< Device detected, PHY communication established
#define PORT_SSTS_IPM_ACTIVE  1 ///< Interface in active power state

#define DMA_OFFSET_CMDLIST  0x000 ///< [0x000..0x3FF] command list (32 slots * 32 bytes)
#define DMA_OFFSET_FIS      0x400 ///< [0x400..0x4FF] received FIS
#define DMA_OFFSET_CMDTABLE 0x500 ///< [0x500..0x57F] command table (FIS at 0x00, ATAPI CDB at 0x40)
#define DMA_OFFSET_PRDT     0x580 ///< [0x580..0x58F] PRDT entry
#define DMA_OFFSET_DATABUF  0x600 ///< [0x600..0x17FF] data buffer (4608 bytes; covers raw CD sectors up to 2448 bytes)

#define DMA_NUM_PAGES 2 ///< DMA pages per port

#define AHCI_POLL_TIMEOUT 1000000

struct ahci_cmd_header {
    uint16_t flags; // CFL:5, A:1, W:1, P:1, R:1, B:1, C:1, rsvd:1, PMP:4
    uint16_t prdtl; // PRDT length in entries
    uint32_t prdbc; // PRD byte count (filled by HBA after transfer)
    uint32_t ctba;  // command table base address low
    uint32_t ctbau; // command table base address upper
    uint32_t reserved[4];
} __packed;

struct ata_fis_h2d {
    uint8_t type;    // ATA_FIS_TYPE_H2D
    uint8_t pm_port; // bit7 = C (new command), bits3:0 = port multiplier port
    uint8_t command;
    uint8_t features;
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t features_ext;
    uint8_t count_lo;
    uint8_t count_hi;
    uint8_t icc;
    uint8_t control;
    uint8_t reserved[4];
} __packed;

struct ahci_prdt_entry {
    uint32_t dba;  // data base address low
    uint32_t dbau; // data base address upper
    uint32_t reserved;
    uint32_t dbc; // bits30:0 = byte count - 1; bit31 = interrupt on completion
} __packed;

static_assert(32 == sizeof(struct ahci_cmd_header));
static_assert(20 == sizeof(struct ata_fis_h2d));
static_assert(16 == sizeof(struct ahci_prdt_entry));

struct ahci_port {
    struct pci_bar* bar;
    uintptr_t port_base;
    uintptr_t dma_phys;
    uint8_t* dma_virt;
};

struct ahci_pci_context {
    struct blk_device devices[32];
    struct atapi_device atapi_devices[32];
    struct ahci_port ports[32];
    uint32_t atapi_mask;
    uint32_t ata_mask;
};

static __always_inline uint32_t ahci_register_read(struct ahci_port* restrict port, uint32_t offset)
{
    return pci_bar_read32(port->bar, port->port_base + offset);
}

static __always_inline void ahci_register_write(struct ahci_port* restrict port, uint32_t offset, uint32_t value)
{
    pci_bar_write32(port->bar, port->port_base + offset, value);
}

static void ahci_stop(struct ahci_port* restrict port)
{
    ahci_register_write(port, PORT_REG_CMD, ahci_register_read(port, PORT_REG_CMD) & ~PORT_CMD_ST);

    while(ahci_register_read(port, PORT_REG_CMD) & PORT_CMD_CR) {
        yield_hart();
    }

    ahci_register_write(port, PORT_REG_CMD, ahci_register_read(port, PORT_REG_CMD) & ~PORT_CMD_FRE);

    while(ahci_register_read(port, PORT_REG_CMD) & PORT_CMD_FR) {
        yield_hart();
    }
}

static void ahci_start(struct ahci_port* restrict port)
{
    ahci_register_write(port, PORT_REG_CMD, ahci_register_read(port, PORT_REG_CMD) | PORT_CMD_FRE);
    ahci_register_write(port, PORT_REG_CMD, ahci_register_read(port, PORT_REG_CMD) | PORT_CMD_ST);
}

static int ahci_poll(struct ahci_port* restrict port, uintptr_t lba)
{
    unsigned counter = 0;
    unsigned long tfd_value;

    while(ahci_register_read(port, PORT_REG_CI) & 1) {
        if(ahci_register_read(port, PORT_REG_TFD) & PORT_TFD_ERR) {
            tfd_value = ahci_register_read(port, PORT_REG_TFD);
            pr_debug("ahci: poll error at LBA %zu (TFD=%08lX)", (size_t)(lba), tfd_value);
            return -EIO;
        }

        if(counter >= AHCI_POLL_TIMEOUT) {
            pr_debug("ahci: poll timeout at LBA %zu", (size_t)(lba));
            return -EIO;
        }

        yield_hart();

        counter += 1;
    }

    if(ahci_register_read(port, PORT_REG_TFD) & PORT_TFD_ERR) {
        tfd_value = ahci_register_read(port, PORT_REG_TFD);
        pr_debug("ahci: poll error at LBA %zu (TFD=%08lX)", (size_t)(lba), tfd_value);
        return -EIO;
    }

    return 0;
}

static void ahci_setup(struct ahci_port* restrict port, uint64_t lba, uint8_t cmd, uint16_t count, bool write)
{
    volatile struct ahci_cmd_header* header = (volatile struct ahci_cmd_header*)(port->dma_virt + DMA_OFFSET_CMDLIST);
    volatile struct ata_fis_h2d* fis = (volatile struct ata_fis_h2d*)(port->dma_virt + DMA_OFFSET_CMDTABLE);
    volatile struct ahci_prdt_entry* prdt = (volatile struct ahci_prdt_entry*)(port->dma_virt + DMA_OFFSET_PRDT);

    uintptr_t cmdtable_phys = port->dma_phys + DMA_OFFSET_CMDTABLE;
    uintptr_t buffer_phys = port->dma_phys + DMA_OFFSET_DATABUF;

    memset((void*)(fis), 0, sizeof(struct ata_fis_h2d));

    if(write) {
        header->flags = host_to_LE16((UINT16_C(1) << 6) | 5); // Write, CFL=5
    }
    else {
        header->flags = host_to_LE16(5); // CFL=5
    }

    header->prdtl = host_to_LE16(1);
    header->prdbc = 0;
    header->ctba = host_to_LE32((uint32_t)(cmdtable_phys));
    header->ctbau = host_to_LE32((uint32_t)(cmdtable_phys >> 32));
    header->reserved[0] = 0;
    header->reserved[1] = 0;
    header->reserved[2] = 0;
    header->reserved[3] = 0;

    fis->type = ATA_FIS_TYPE_H2D;
    fis->pm_port = 0x80; // C=1, new command
    fis->command = cmd;
    fis->device = ATA_DEVICE_LBA;
    fis->lba0 = (uint8_t)(lba >> 0);
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);
    fis->count_lo = (uint8_t)(count >> 0);
    fis->count_hi = (uint8_t)(count >> 8);

    prdt->dba = host_to_LE32((uint32_t)(buffer_phys));
    prdt->dbau = host_to_LE32((uint32_t)(buffer_phys >> 32));
    prdt->reserved = 0;
    prdt->dbc = host_to_LE32(512 - 1);
}

static int ahci_issue(struct ahci_port* restrict port, uintptr_t lba)
{
    ahci_register_write(port, PORT_REG_SERR, 0xFFFFFFFF);
    ahci_register_write(port, PORT_REG_IS, 0xFFFFFFFF);

    while(ahci_register_read(port, PORT_REG_TFD) & (PORT_TFD_BSY | PORT_TFD_DRQ)) {
        yield_hart();
    }

    ahci_register_write(port, PORT_REG_CI, 1);

    return ahci_poll(port, lba);
}

static int ahci_identify(struct ahci_port* restrict port, ata_identify_type identify)
{
    volatile uint8_t* databuf = (volatile uint8_t*)(port->dma_virt + DMA_OFFSET_DATABUF);

    int retval;

    memset((void*)(databuf), 0, 512);

    ahci_setup(port, 0, ATA_CMD_IDENTIFY, 0, false);

    retval = ahci_issue(port, 0);

    if(retval < 0) {
        return retval;
    }

    memcpy(identify, (const void*)(databuf), sizeof(ata_identify_type));

    return 0;
}

static int ahci_atapi_identify(struct ahci_port* restrict port, ata_identify_type identify)
{
    volatile uint8_t* databuf = (volatile uint8_t*)(port->dma_virt + DMA_OFFSET_DATABUF);

    int retval;

    memset((void*)(databuf), 0, sizeof(ata_identify_type));

    ahci_setup(port, 0, ATA_CMD_IDENTIFY_ATAPI, 0, false);

    retval = ahci_issue(port, 0);

    if(retval < 0) {
        return retval;
    }

    memcpy(identify, (const void*)(databuf), sizeof(ata_identify_type));

    return 0;
}

static void ahci_atapi_setup(struct ahci_port* restrict port, const atapi_cdb_type cdb, size_t byte_count, bool write)
{
    volatile struct ahci_cmd_header* header = (volatile struct ahci_cmd_header*)(port->dma_virt + DMA_OFFSET_CMDLIST);
    volatile struct ata_fis_h2d* fis = (volatile struct ata_fis_h2d*)(port->dma_virt + DMA_OFFSET_CMDTABLE);
    volatile struct ahci_prdt_entry* prdt = (volatile struct ahci_prdt_entry*)(port->dma_virt + DMA_OFFSET_PRDT);

    volatile uint8_t* atapi_area = port->dma_virt + DMA_OFFSET_CMDTABLE + 0x40;

    uintptr_t cmdtable_phys = port->dma_phys + DMA_OFFSET_CMDTABLE;
    uintptr_t buffer_phys = port->dma_phys + DMA_OFFSET_DATABUF;

    memset((void*)(fis), 0, sizeof(struct ata_fis_h2d));

    if(write) {
        header->flags = host_to_LE16((UINT16_C(1) << 6) | (UINT16_C(1) << 5) | 5); // Write, ATAPI, CFL=5
    }
    else {
        header->flags = host_to_LE16((UINT16_C(1) << 5) | 5); // ATAPI, CFL=5
    }

    header->prdtl = host_to_LE16(byte_count > 0 ? 1 : 0);
    header->prdbc = 0;
    header->ctba = host_to_LE32((uint32_t)(cmdtable_phys));
    header->ctbau = host_to_LE32((uint32_t)(cmdtable_phys >> 32));
    header->reserved[0] = 0;
    header->reserved[1] = 0;
    header->reserved[2] = 0;
    header->reserved[3] = 0;

    fis->type = ATA_FIS_TYPE_H2D;
    fis->pm_port = 0x80;
    fis->command = ATA_CMD_PACKET;
    fis->features = 0x01; // DMA
    fis->lba1 = (uint8_t)(byte_count & 0xFF);
    fis->lba2 = (uint8_t)((byte_count >> 8) & 0xFF);

    memcpy((void*)(atapi_area), cdb, sizeof(atapi_cdb_type));

    if(byte_count > 0) {
        prdt->dba = host_to_LE32((uint32_t)(buffer_phys));
        prdt->dbau = host_to_LE32((uint32_t)(buffer_phys >> 32));
        prdt->reserved = 0;
        prdt->dbc = host_to_LE32((uint32_t)(byte_count - 1));
    }
}

static int ahci_atapi_read_capacity(struct ahci_port* restrict port, size_t* restrict sector_count, size_t* restrict sector_size)
{
    volatile uint8_t* databuf = (volatile uint8_t*)(port->dma_virt + DMA_OFFSET_DATABUF);

    atapi_cdb_type cdb;
    uint8_t result[8];
    int retval;

    memset(cdb, 0, sizeof(atapi_cdb_type));
    cdb[0] = 0x25; // READ CAPACITY(10)

    memset((void*)(databuf), 0, sizeof(result));

    ahci_atapi_setup(port, cdb, sizeof(result), false);

    retval = ahci_issue(port, 0);

    if(retval < 0) {
        return retval;
    }

    memcpy(result, (const void*)(databuf), sizeof(result));

    sector_count[0] = (size_t)(result[0]) << 24;
    sector_count[0] |= (size_t)(result[1]) << 16;
    sector_count[0] |= (size_t)(result[2]) << 8;
    sector_count[0] |= (size_t)(result[3]);
    sector_count[0] += 1;

    sector_size[0] = (size_t)(result[4]) << 24;
    sector_size[0] |= (size_t)(result[5]) << 16;
    sector_size[0] |= (size_t)(result[6]) << 8;
    sector_size[0] |= (size_t)(result[7]);

    return 0;
}

static int ahci_atapi_send_packet(const struct atapi_device* restrict device, const atapi_cdb_type cdb, void* restrict buffer, size_t count)
{
    struct ahci_port* port = device->base.ctxdata;

    volatile uint8_t* databuf = (volatile uint8_t*)(port->dma_virt + DMA_OFFSET_DATABUF);

    bool write = (bool)(cdb[0] == 0x2A); // WRITE(10)
    int retval;

    if(write && count > 0) {
        memcpy((void*)(databuf), buffer, count);
    }

    ahci_atapi_setup(port, cdb, count, write);

    retval = ahci_issue(port, 0);

    if(retval < 0) {
        return retval;
    }

    if(!write && count > 0) {
        memcpy(buffer, (const void*)(databuf), count);
    }

    return 0;
}

static const struct atapi_device_ops ahci_atapi_ops = {
    .send_packet = &ahci_atapi_send_packet,
};

static int ahci_read(const struct blk_device* restrict device, uintptr_t lba, size_t count, void* restrict buffer)
{
    struct ahci_port* port = device->ctxdata;

    volatile uint8_t* databuf = (volatile uint8_t*)(port->dma_virt + DMA_OFFSET_DATABUF);

    uint8_t* buffer_ptr;
    uintptr_t current_lba;
    size_t sector;
    int retval;

    for(sector = 0; sector < count; sector += 1) {
        current_lba = lba + sector;

        ahci_setup(port, current_lba, ATA_CMD_READ_DMA_EXT, 1, false);

        retval = ahci_issue(port, current_lba);

        if(retval < 0) {
            return retval;
        }

        buffer_ptr = buffer;
        buffer_ptr += sector * 512;
        memcpy(buffer_ptr, (const uint8_t*)(databuf), 512);
    }

    return 0;
}

static int ahci_write(const struct blk_device* restrict device, uintptr_t lba, size_t count, const void* restrict buffer)
{
    struct ahci_port* port = device->ctxdata;

    volatile uint8_t* databuf = (volatile uint8_t*)(port->dma_virt + DMA_OFFSET_DATABUF);

    const uint8_t* buffer_ptr;
    uintptr_t current_lba;
    size_t sector;
    int retval;

    for(sector = 0; sector < count; sector += 1) {
        current_lba = lba + sector;

        buffer_ptr = buffer;
        buffer_ptr += sector * 512;
        memcpy((uint8_t*)(databuf), buffer_ptr, 512);

        ahci_setup(port, current_lba, ATA_CMD_WRITE_DMA_EXT, 1, true);

        retval = ahci_issue(port, current_lba);

        if(retval < 0) {
            return retval;
        }
    }

    return 0;
}

static int ahci_flush(const struct blk_device* restrict device)
{
    struct ahci_port* port = device->ctxdata;

    ahci_setup(port, 0, ATA_CMD_FLUSH_EXT, 0, false);

    return ahci_issue(port, 0);
}

static int ahci_port_init(struct ahci_pci_context* restrict context, struct pci_bar* restrict abar, unsigned port_num)
{
    struct ahci_port* port = &context->ports[port_num];

    uintptr_t port_base = HBA_PORT_BASE + port_num * HBA_PORT_SIZE;
    uintptr_t dma_phys;
    uint8_t* dma_virt;

    dma_phys = pdma_alloc(DMA_NUM_PAGES);

    if(dma_phys == 0) {
        return -ENOMEM;
    }

    dma_virt = phys_to_virt(dma_phys);

    memset(dma_virt, 0, PAGE_SIZE * DMA_NUM_PAGES);

    port->bar = abar;
    port->port_base = port_base;
    port->dma_phys = dma_phys;
    port->dma_virt = dma_virt;

    ahci_stop(port);

    pci_bar_write32(abar, port_base + PORT_REG_CLB, (uint32_t)(dma_phys + DMA_OFFSET_CMDLIST));
    pci_bar_write32(abar, port_base + PORT_REG_CLBU, (uint32_t)((dma_phys + DMA_OFFSET_CMDLIST) >> 32));
    pci_bar_write32(abar, port_base + PORT_REG_FB, (uint32_t)(dma_phys + DMA_OFFSET_FIS));
    pci_bar_write32(abar, port_base + PORT_REG_FBU, (uint32_t)((dma_phys + DMA_OFFSET_FIS) >> 32));

    pci_bar_write32(abar, port_base + PORT_REG_SERR, 0xFFFFFFFF);
    pci_bar_write32(abar, port_base + PORT_REG_IS, 0xFFFFFFFF);

    ahci_start(port);

    return 0;
}

static void ahci_port_teardown(struct ahci_port* restrict port)
{
    ahci_stop(port);

    pdma_free(port->dma_phys, DMA_NUM_PAGES);

    port->dma_phys = 0;
}

static const struct blk_device_ops ahci_blk_device_ops = {
    .read = &ahci_read,
    .write = &ahci_write,
    .flush = &ahci_flush,
    .destroy = nullptr,
};

static int ahci_register_ata_port(struct ahci_pci_context* restrict context, unsigned iterator, unsigned port_num)
{
    struct blk_device* device = &context->devices[port_num];

    device->ops = &ahci_blk_device_ops;
    device->sector_count = 0;
    device->sector_size = 0;
    device->flags = 0;
    device->ctxdata = nullptr;

    return blk_device_register_root(device, blk_name_ata, iterator, port_num);
}

static int ahci_register_atapi_port(struct ahci_pci_context* restrict context, unsigned iterator, unsigned port_num)
{
    struct atapi_device* atapi_dev = &context->atapi_devices[port_num];

    atapi_dev->ops = &ahci_atapi_ops;
    atapi_dev->flags = 0;
    atapi_dev->base.ctxdata = nullptr;
    atapi_dev->base.sector_count = 0;
    atapi_dev->base.sector_size = 0;
    atapi_dev->base.flags = 0;

    return atapi_register(atapi_dev, blk_name_atapi, iterator, port_num);
}

static void ahci_probe_ata_port(struct ahci_pci_context* restrict context, struct pci_bar* restrict abar, unsigned port_num)
{
    ata_identify_type identify;
    struct ahci_port* port;
    struct blk_device* device;
    char model_str[41];

    if(ahci_port_init(context, abar, port_num)) {
        pr_debug("ahci: out of DMA memory for port %u", port_num);
        return;
    }

    port = &context->ports[port_num];

    if(ahci_identify(port, identify)) {
        pr_debug("ahci: identify failed on port %u", port_num);
        ahci_port_teardown(port);
        return;
    }

    if(ata_identify_is_atapi(identify)) {
        ahci_port_teardown(port);
        return;
    }

    device = &context->devices[port_num];
    device->sector_count = ata_identify_sector_count(identify);
    device->sector_size = ata_identify_logical_sector_size(identify);
    device->flags = BLK_FLAG_PRESENT;
    device->ctxdata = port;

    ata_identify_string(&identify[ATA_IDENTIFY_MODEL_FIELD], ATA_IDENTIFY_MODEL_NWORDS, model_str, sizeof(model_str));
    pr_debug("ahci: port %u: %s (%zu sectors)", port_num, model_str, device->sector_count);

    blk_event_notify(BLK_EVENT_MEDIA_CONNECTED, device);
}

static void ahci_probe_atapi_port(struct ahci_pci_context* restrict context, struct pci_bar* restrict abar, unsigned port_num)
{
    ata_identify_type identify;
    struct ahci_port* port;
    struct atapi_device* atapi_dev;
    size_t sector_count;
    size_t sector_size;
    char model_str[41];

    if(ahci_port_init(context, abar, port_num)) {
        pr_debug("ahci: out of DMA memory for ATAPI port %u", port_num);
        return;
    }

    port = &context->ports[port_num];
    atapi_dev = &context->atapi_devices[port_num];

    if(0 == ahci_atapi_identify(port, identify)) {
        ata_identify_string(&identify[ATA_IDENTIFY_MODEL_FIELD], ATA_IDENTIFY_MODEL_NWORDS, model_str, sizeof(model_str));
    }
    else {
        model_str[0] = 0x00;
    }

    if(ahci_atapi_read_capacity(port, &sector_count, &sector_size)) {
        pr_debug("ahci: ATAPI port %u: no media or read capacity failed", port_num);
        ahci_port_teardown(port);
        return;
    }

    atapi_dev->base.sector_count = sector_count;
    atapi_dev->base.sector_size = sector_size;
    atapi_dev->base.flags = BLK_FLAG_PRESENT;
    atapi_dev->base.ctxdata = port;

    pr_debug("ahci: port %u ATAPI: %s (%zu sectors of %zu bytes)", port_num, model_str, sector_count, sector_size);

    blk_event_notify(BLK_EVENT_MEDIA_CONNECTED, &atapi_dev->base);
}

static int ahci_driver_probe(struct pci_device* restrict pci_dev, const struct pci_driver_id* restrict id)
{
    struct ahci_pci_context* context;
    struct pci_bar* abar;
    uint32_t ahci_pi;
    uint32_t ahci_ghc;
    uint32_t ahci_ssts;
    uint32_t ahci_signature;
    uint32_t port_bit;
    uintptr_t port_base;
    unsigned port_num;
    unsigned iterator;

    context = kmalloc(sizeof(struct ahci_pci_context));

    if(context == nullptr) {
        pr_debug("ahci: out of memory for context");
        return -ENOMEM;
    }

    memset(context, 0, sizeof(struct ahci_pci_context));

    pci_dev->ctxdata = context;

    pci_enable_busmaster(pci_dev);

    abar = pci_bar_map(pci_dev, 5);

    if(abar == nullptr) {
        pr_debug("ahci: pci_bar_map failed for ABAR");
        kfree(context);
        pci_dev->ctxdata = nullptr;
        return -ENOMEM;
    }

    ahci_ghc = pci_bar_read32(abar, HBA_GHC);

    if(0 == (ahci_ghc & HBA_GHC_AE)) {
        pci_bar_write32(abar, HBA_GHC, ahci_ghc | HBA_GHC_AE);
    }

    ahci_pi = pci_bar_read32(abar, HBA_PI);

    iterator = blk_name_increment(blk_name_ata);

    for(port_num = 0; port_num < 32; port_num += 1) {
        if(0 == (ahci_pi & (UINT32_C(1) << port_num))) {
            continue;
        }

        port_base = HBA_PORT_BASE + port_num * HBA_PORT_SIZE;
        ahci_signature = pci_bar_read32(abar, port_base + PORT_REG_SIG);

        if(ahci_signature == ATA_SIGNATURE_ATAPI) {
            if(0 == ahci_register_atapi_port(context, iterator, port_num)) {
                context->atapi_mask |= (UINT32_C(1) << port_num);
            }
        }
        else {
            if(0 == ahci_register_ata_port(context, iterator, port_num)) {
                context->ata_mask |= (UINT32_C(1) << port_num);
            }
        }
    }

    if(0 == (context->ata_mask | context->atapi_mask)) {
        pci_bar_unmap(pci_dev, 5);
        kfree(context);
        pci_dev->ctxdata = nullptr;
        return -ENODEV;
    }

    for(port_num = 0; port_num < 32; port_num += 1) {
        port_bit = UINT32_C(1) << port_num;

        if(0 == ((context->ata_mask | context->atapi_mask) & port_bit)) {
            continue;
        }

        port_base = HBA_PORT_BASE + port_num * HBA_PORT_SIZE;
        ahci_ssts = pci_bar_read32(abar, port_base + PORT_REG_SSTS);

        if((ahci_ssts & 0x0F) != PORT_SSTS_DET_PRESENT) {
            continue;
        }

        if(((ahci_ssts >> 8) & 0x0F) != PORT_SSTS_IPM_ACTIVE) {
            continue;
        }

        if(context->atapi_mask & port_bit) {
            ahci_probe_atapi_port(context, abar, port_num);
        }
        else {
            ahci_probe_ata_port(context, abar, port_num);
        }
    }

    return 0;
}

static void ahci_driver_remove(struct pci_device* restrict pci_dev)
{
    struct ahci_pci_context* context = pci_dev->ctxdata;
    unsigned port_num;
    uint32_t port_bit;

    if(context == nullptr) {
        return;
    }

    for(port_num = 0; port_num < 32; port_num += 1) {
        port_bit = UINT32_C(1) << port_num;

        if(context->ata_mask & port_bit) {
            blk_device_unregister(&context->devices[port_num]);
        }
        else if(context->atapi_mask & port_bit) {
            atapi_unregister(&context->atapi_devices[port_num]);
        }
        else {
            continue;
        }

        if(context->ports[port_num].dma_phys) {
            ahci_port_teardown(&context->ports[port_num]);
        }
    }

    pci_bar_unmap(pci_dev, 5);

    kfree(context);

    pci_dev->ctxdata = nullptr;
}

static const struct pci_driver_ops ahci_driver_ops = {
    .probe = &ahci_driver_probe,
    .remove = &ahci_driver_remove,
};

static const struct pci_driver_id ahci_ids[] = {
    PCI_DEVICE_CLASS(0x01, 0x06),
    PCI_DEVICE_END(),
};

static struct pci_driver ahci_driver = {
    .ops = &ahci_driver_ops,
    .ids = ahci_ids,
};

define_initcall(ahci, INITCALL_STAGE_BSP_DEV, INITCALL_ORDER_MIDDLE)
{
    pci_driver_register(&ahci_driver);
}
