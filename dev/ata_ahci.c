#include <arch/hart.h>
#include <bus/pci.h>
#include <core/assert.h>
#include <core/endian.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <dev/ata.h>
#include <dev/block_device.h>
#include <dev/block_namespace.h>
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
#define DMA_OFFSET_CMDTABLE 0x500 ///< [0x500..0x57F] command table
#define DMA_OFFSET_PRDT     0x580 ///< [0x580..0x58F] PRDT entry
#define DMA_OFFSET_DATABUF  0x600 ///< [0x600..0x7FF] data buffer (512 bytes)

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

struct ahci_port {
    struct pci_bar* bar;
    uintptr_t port_base;
    uintptr_t dma_phys;
    uint8_t* dma_virt;
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
    unsigned timeout = AHCI_POLL_TIMEOUT;

    while(ahci_register_read(port, PORT_REG_CI) & 1) {
        if(ahci_register_read(port, PORT_REG_TFD) & PORT_TFD_ERR) {
            pr_error("ahci: error at lba %llu (tfd=%08X)", (unsigned long long)(lba), ahci_register_read(port, PORT_REG_TFD));
            return EIO;
        }

        if(0 == timeout--) {
            pr_error("ahci: timeout at lba %llu", (unsigned long long)(lba));
            return EIO;
        }

        yield_hart();
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

    header->flags = host_to_LE16((write ? (UINT16_C(1) << 6) : 0) | 5); // W, CFL=5
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

    if(retval) {
        return retval;
    }

    memcpy(identify, (const void*)(databuf), sizeof(ata_identify_type));
    return 0;
}

static int ahci_op_read(struct block_device* restrict device, uintptr_t lba, size_t count, void* restrict buffer)
{
    struct ahci_port* port = device->ctxdata;
    volatile uint8_t* databuf = (volatile uint8_t*)(port->dma_virt + DMA_OFFSET_DATABUF);
    uintptr_t current_lba;
    size_t sector;
    int retval;

    for(sector = 0; sector < count; sector += 1) {
        current_lba = lba + sector;

        ahci_setup(port, current_lba, ATA_CMD_READ_DMA_EXT, 1, false);

        retval = ahci_issue(port, current_lba);

        if(retval) {
            return retval;
        }

        memcpy((uint8_t*)(buffer) + sector * 512, (const uint8_t*)(databuf), 512);
    }

    return 0;
}

static int ahci_op_write(struct block_device* restrict device, uintptr_t lba, size_t count, const void* restrict buffer)
{
    struct ahci_port* port = device->ctxdata;
    volatile uint8_t* databuf = (volatile uint8_t*)(port->dma_virt + DMA_OFFSET_DATABUF);
    uintptr_t current_lba;
    size_t sector;
    int retval;

    for(sector = 0; sector < count; sector += 1) {
        current_lba = lba + sector;

        memcpy((uint8_t*)(databuf), (const uint8_t*)(buffer) + sector * 512, 512);

        ahci_setup(port, current_lba, ATA_CMD_WRITE_DMA_EXT, 1, true);

        retval = ahci_issue(port, current_lba);

        if(retval) {
            return retval;
        }
    }

    return 0;
}

static int ahci_op_flush(struct block_device* restrict device)
{
    struct ahci_port* port = device->ctxdata;

    ahci_setup(port, 0, ATA_CMD_FLUSH_EXT, 0, false);

    return ahci_issue(port, 0);
}

static const struct block_device_ops ahci_block_device_ops = {
    .read = &ahci_op_read,
    .write = &ahci_op_write,
    .flush = &ahci_op_flush,
    .destroy = nullptr,
};

static int ahci_probe(struct pci_device* restrict pci_dev, const struct pci_driver_id* restrict id)
{
    ata_identify_type identify;
    struct ahci_port* port;
    struct block_device* device;
    struct pci_bar* abar;
    uint32_t pi, ghc, ssts;
    unsigned port_num;
    uintptr_t port_base, dma_phys;
    uint8_t* dma_virt;
    int found = 0;
    char model[41];

    pci_enable_busmaster(pci_dev);

    abar = pci_bar_map(pci_dev, 5);

    if(abar == nullptr) {
        pr_warning("ahci: failed to map abar");
        return ENOMEM;
    }

    ghc = pci_bar_read32(abar, HBA_GHC);

    if(0 == (ghc & HBA_GHC_AE)) {
        pci_bar_write32(abar, HBA_GHC, ghc | HBA_GHC_AE);
    }

    pi = pci_bar_read32(abar, HBA_PI);

    for(port_num = 0; port_num < 32; port_num += 1) {
        if(0 == (pi & (UINT32_C(1) << port_num))) {
            continue;
        }

        port_base = HBA_PORT_BASE + port_num * HBA_PORT_SIZE;
        ssts = pci_bar_read32(abar, port_base + PORT_REG_SSTS);

        if((ssts & 0x0F) != PORT_SSTS_DET_PRESENT) {
            continue;
        }

        if(((ssts >> 8) & 0x0F) != PORT_SSTS_IPM_ACTIVE) {
            continue;
        }

        if(pci_bar_read32(abar, port_base + PORT_REG_SIG) == ATA_SIGNATURE_ATAPI) {
            pr_info("ahci: port %u is ATAPI, skipping for now", port_num);
            continue;
        }

        if(pci_bar_read32(abar, port_base + PORT_REG_SIG) != ATA_SIGNATURE_ATA) {
            continue;
        }

        dma_phys = pdma_alloc(1);

        if(dma_phys == 0) {
            pr_error("ahci: out of dma memory for port %u", port_num);
            continue;
        }

        dma_virt = phys_to_virt(dma_phys);
        memset(dma_virt, 0, PAGE_SIZE);

        port = kmalloc(sizeof(struct ahci_port));

        if(port == nullptr) {
            pdma_free(dma_phys, 1);
            continue;
        }

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

        if(ahci_identify(port, identify)) {
            pr_error("ahci: identify failed on port %u", port_num);
            ahci_stop(port);
            pdma_free(dma_phys, 1);
            kfree(port);
            continue;
        }

        if(ata_identify_is_atapi(identify)) {
            pr_info("ahci: port %u is ATAPI (via identify), skipping for now", port_num);
            ahci_stop(port);
            pdma_free(dma_phys, 1);
            kfree(port);
            continue;
        }

        device = kmalloc(sizeof(struct block_device));

        if(device == nullptr) {
            ahci_stop(port);
            pdma_free(dma_phys, 1);
            kfree(port);
            continue;
        }

        memset(device, 0, sizeof(struct block_device));

        device->ops = &ahci_block_device_ops;
        device->sector_count = ata_identify_sector_count(identify);
        device->sector_size = ata_identify_logical_sector_size(identify);
        device->ctxdata = port;

        ata_identify_string(identify + ATA_IDENTIFY_MODEL_FIELD, ATA_IDENTIFY_MODEL_NWORDS, model, sizeof(model));
        pr_info("ahci: port %u: %s (%zu sectors)", port_num, model, device->sector_count);

        if(block_device_register(device, block_namespace_ata)) {
            ahci_stop(port);
            pdma_free(dma_phys, 1);
            kfree(port);
            kfree(device);
            continue;
        }

        found += 1;
    }

    return found ? 0 : ENODEV;
}

static void ahci_remove(struct pci_device* restrict pci_dev)
{
    (void)(pci_dev);
}

static const struct pci_driver_ops ahci_driver_ops = {
    .probe = &ahci_probe,
    .remove = &ahci_remove,
};

static const struct pci_driver_id ahci_ids[] = {
    PCI_DEVICE_CLASS(0x01, 0x06),        // Generic AHCI (class 01h, subclass 06h)
    PCI_DEVICE_SPECIFIC(0x8086, 0x2922), // Intel ICH9 AHCI
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
