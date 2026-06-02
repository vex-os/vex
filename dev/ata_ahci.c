#include <arch/hart.h>
#include <bus/pci.h>
#include <core/assert.h>
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

#if 0

// VIBECODE AHEAD VIBECODE AHEAD VIBECODE AHEAD VIBECODE AHEAD
// SUPERVISED, BUT VIBECODE NONETHELESS
// TODO: rewrite, make a proper wrapper around ATA commands

#define HBA_GHC    0x04                ///< Global Host Control
#define HBA_PI     0x0C                ///< Ports Implemented
#define HBA_GHC_AE (UINT32_C(1) << 31) ///< AHCI Enable

// Port register offsets from port base
#define PORT_CLB   0x00                ///< Command List Base Address
#define PORT_CLBU  0x04                ///< Command List Base Address Upper 32 bits
#define PORT_FB    0x08                ///< FIS Base Address
#define PORT_FBU   0x0C                ///< FIS Base Address Upper 32 bits
#define PORT_IS    0x10                ///< Interrupt Status
#define PORT_CMD   0x18                ///< Command and Status
#define PORT_TFD   0x20                ///< Task File Data
#define PORT_SIG   0x24                ///< Signature
#define PORT_SSTS  0x28                ///< SATA Status (SCR0)
#define PORT_SERR  0x30                ///< SATA Error (SCR1)
#define PORT_CI    0x38                ///< Command Issue

#define PORT_CMD_ST  (UINT32_C(1) << 0)  ///< Start
#define PORT_CMD_FRE (UINT32_C(1) << 4)  ///< FIS Receive Enable
#define PORT_CMD_FR  (UINT32_C(1) << 14) ///< FIS Receive Running
#define PORT_CMD_CR  (UINT32_C(1) << 15) ///< Command List Running
#define PORT_TFD_ERR (UINT32_C(1) << 0)  ///< Task File Error

#define MEM_CMDLIST  0x000 ///< [0x000..0x3FF] command list
#define MEM_FIS      0x400 ///< [0x400..0x4FF] received FIS
#define MEM_CMDTABLE 0x500 ///< [0x500..0x57F] command table
#define MEM_PRDT     0x580 ///< [0x580..0x58F] PRDT entry
#define MEM_DATABUF  0x600 ///< [0x600..0x7FF] data buffer

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
    ahci_register_write(port, PORT_CMD, ahci_register_read(port, PORT_CMD) & ~PORT_CMD_ST);

    while(ahci_register_read(port, PORT_CMD) & PORT_CMD_CR) {
        yield_hart();
    }

    ahci_register_write(port, PORT_CMD, ahci_register_read(port, PORT_CMD) & ~PORT_CMD_FRE);

    while(ahci_register_read(port, PORT_CMD) & PORT_CMD_FR) {
        yield_hart();
    }
}

static void ahci_start(struct ahci_port* restrict port)
{
    ahci_register_write(port, PORT_CMD, ahci_register_read(port, PORT_CMD) | PORT_CMD_FRE);
    ahci_register_write(port, PORT_CMD, ahci_register_read(port, PORT_CMD) | PORT_CMD_ST);
}

static int ahci_poll(struct ahci_port* restrict port, uintptr_t lba)
{
    while(ahci_register_read(port, PORT_CI) & 1) {
        if(ahci_register_read(port, PORT_TFD) & PORT_TFD_ERR) {
            pr_error("ahci: error at lba %llu (tfd=%08X)", (unsigned long long)(lba), ahci_register_read(port, PORT_TFD));
            return EIO;
        }
    }

    return 0;
}

static void ahci_setup(struct ahci_port* restrict port, uint64_t lba, uint8_t cmd, bool write)
{
    volatile uint32_t* cmdlist = (volatile uint32_t*)(port->dma_virt + MEM_CMDLIST);
    volatile uint8_t* cmdtable = (volatile uint8_t*)(port->dma_virt + MEM_CMDTABLE);
    volatile uint32_t* prdt = (volatile uint32_t*)(port->dma_virt + MEM_PRDT);
    uintptr_t cmdtable_phys = port->dma_phys + MEM_CMDTABLE;
    uintptr_t buffer_phys = port->dma_phys + MEM_DATABUF;
    size_t i;

    for(i = 0; i < 0x90; ++i) {
        cmdtable[i] = 0; // zero out command table
    }

    if(write) {
        cmdlist[0] = (UINT32_C(1) << 16) | (UINT32_C(1) << 6) | 5; // PRDTL=1, W=1, CFL=5
    }
    else {
        cmdlist[0] = (UINT32_C(1) << 16) | 5; // PRDTL=1, W=0, CFL=5
    }

    cmdlist[1] = 0; // no flags
    cmdlist[2] = (uint32_t)(cmdtable_phys);
    cmdlist[3] = (uint32_t)(cmdtable_phys >> 32);
    cmdlist[4] = 0;
    cmdlist[5] = 0;
    cmdlist[6] = 0;
    cmdlist[7] = 0;

    cmdtable[0] = ATA_FIS_TYPE_H2D;
    cmdtable[1] = 0x80; // PM=1
    cmdtable[2] = cmd;
    cmdtable[3] = 0;
    cmdtable[4] = (uint8_t)(lba >> 0);
    cmdtable[5] = (uint8_t)(lba >> 8);
    cmdtable[6] = (uint8_t)(lba >> 16);
    cmdtable[7] = 0x40; // LBA mode
    cmdtable[8] = (uint8_t)(lba >> 24);
    cmdtable[9] = (uint8_t)(lba >> 32);
    cmdtable[10] = (uint8_t)(lba >> 40);
    cmdtable[11] = 0;
    cmdtable[12] = 1; // sector count
    cmdtable[13] = 0;

    prdt[0] = (uint32_t)(buffer_phys);
    prdt[1] = (uint32_t)(buffer_phys >> 32);
    prdt[2] = 0;
    prdt[3] = 512 - 1; // 512 bytes per sector
}

static int ahci_op_read(struct block_device* restrict device, uintptr_t lba, size_t count, void* restrict buffer)
{
    struct ahci_port* port = device->ctxdata;
    volatile uint8_t* databuf = (volatile uint8_t*)(port->dma_virt + MEM_DATABUF);
    uintptr_t current_lba;
    size_t sector;
    int retval;

    for(sector = 0; sector < count; sector += 1) {
        current_lba = lba + sector;

        ahci_setup(port, current_lba, ATA_CMD_READ_DMA_EXT, false);

        ahci_register_write(port, PORT_SERR, 0xFFFFFFFF);
        ahci_register_write(port, PORT_IS, 0xFFFFFFFF);

        while(ahci_register_read(port, PORT_TFD) & (UINT32_C(1) << 7 | UINT32_C(1) << 3)) {
            yield_hart();
        }

        ahci_register_write(port, PORT_CI, 1);

        retval = ahci_poll(port, current_lba);

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
    volatile uint8_t* databuf = (volatile uint8_t*)(port->dma_virt + MEM_DATABUF);
    uintptr_t current_lba;
    size_t sector;
    int retval;

    for(sector = 0; sector < count; sector += 1) {
        current_lba = lba + sector;

        memcpy((uint8_t*)(databuf), (const uint8_t*)(buffer) + sector * 512, 512);

        ahci_setup(port, current_lba, ATA_CMD_WRITE_DMA_EXT, true);

        ahci_register_write(port, PORT_SERR, 0xFFFFFFFF);
        ahci_register_write(port, PORT_IS, 0xFFFFFFFF);

        while(ahci_register_read(port, PORT_TFD) & (UINT32_C(1) << 7 | UINT32_C(1) << 3)) {
            yield_hart();
        }

        ahci_register_write(port, PORT_CI, 1);

        retval = ahci_poll(port, current_lba);

        if(retval) {
            return retval;
        }
    }

    return 0;
}

static int ahci_op_flush(struct block_device* restrict device)
{
    struct ahci_port* port = device->ctxdata;

    ahci_setup(port, 0, ATA_CMD_FLUSH_EXT, false);

    ahci_register_write(port, PORT_SERR, 0xFFFFFFFF);
    ahci_register_write(port, PORT_IS, 0xFFFFFFFF);

    while(ahci_register_read(port, PORT_TFD) & (UINT32_C(1) << 7 | UINT32_C(1) << 3)) {
        yield_hart();
    }

    ahci_register_write(port, PORT_CI, 1);

    return ahci_poll(port, 0);
}

static const struct block_device_ops ahci_block_device_ops = {
    .read = &ahci_op_read,
    .write = &ahci_op_write,
    .flush = &ahci_op_flush,
    .destroy = nullptr,
};

static int ahci_probe(struct pci_device* restrict pci_dev, const struct pci_driver_id* restrict id)
{
    struct ahci_port* port;
    struct block_device* device;
    struct pci_bar* abar;
    uint32_t pi, ghc, ssts;
    unsigned port_num;
    uintptr_t port_base, dma_phys;
    uint8_t* dma_virt;

    abar = pci_bar_map(pci_dev, 5);

    if(abar == nullptr) {
        pr_warning("ahci: failed to map abar");
        return ENOMEM;
    }

    ghc = pci_bar_read32(abar, HBA_GHC);

    if(0 == (ghc & HBA_GHC_AE)) {
        pci_bar_write32(abar, HBA_GHC, ghc | HBA_GHC_AE); // Ensure AHCI mode is enabled
    }

    pi = pci_bar_read32(abar, HBA_PI);

    for(port_num = 0; port_num < 32; port_num += 1) {
        if(0 == (pi & (UINT32_C(1) << port_num))) {
            continue; // Port not implemented
        }

        port_base = 0x100 + port_num * 0x80;
        ssts = pci_bar_read32(abar, port_base + PORT_SSTS);

        if((ssts & 0x0F) != 3) {
            continue; // No device detected
        }

        if(((ssts >> 8) & 0x0F) != 1) {
            continue; // Interface not active
        }

        if(pci_bar_read32(abar, port_base + PORT_SIG) != ATA_SIGNATURE_ATA) {
            continue; // Not an ATA drive
        }

        dma_phys = pdma_alloc(1);

        if(dma_phys == 0) {
            pr_error("ahci: out of dma memory for port %u", port_num);
            continue;
        }

        dma_virt = phys_to_virt(dma_phys);
        port = kmalloc(sizeof(struct ahci_port));

        if(port == nullptr) {
            pdma_free(dma_phys, 1);
            continue;
        }

        memset(port, 0, sizeof(struct ahci_port));

        port->bar = abar;
        port->port_base = port_base;
        port->dma_phys = dma_phys;
        port->dma_virt = dma_virt;

        ahci_stop(port);

        pci_bar_write32(abar, port_base + PORT_CLB, (uint32_t)(dma_phys + MEM_CMDLIST));
        pci_bar_write32(abar, port_base + PORT_CLBU, (uint32_t)((dma_phys + MEM_CMDLIST) >> 32));
        pci_bar_write32(abar, port_base + PORT_FB, (uint32_t)(dma_phys + MEM_FIS));
        pci_bar_write32(abar, port_base + PORT_FBU, (uint32_t)((dma_phys + MEM_FIS) >> 32));

        pci_bar_write32(abar, port_base + PORT_SERR, 0xFFFFFFFF);
        pci_bar_write32(abar, port_base + PORT_IS, 0xFFFFFFFF);

        ahci_start(port);

        device = kmalloc(sizeof(struct block_device));

        if(device == nullptr) {
            pdma_free(dma_phys, 1);
            kfree(port);
            continue;
        }

        memset(device, 0, sizeof(struct block_device));

        device->ops = &ahci_block_device_ops;
        device->sector_count = 0; // unknown without IDENTIFY
        device->sector_size = 512;
        device->ctxdata = port;

        return block_device_register(device, block_namespace_ata);
    }

    return ENODEV;
}

static void ahci_remove(struct pci_device* restrict pci_dev)
{
    // emoty
}

static const struct pci_driver_ops ahci_driver_ops = {
    .probe = &ahci_probe,
    .remove = &ahci_remove,
};

static const struct pci_driver_id ahci_ids[] = {
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
#endif
