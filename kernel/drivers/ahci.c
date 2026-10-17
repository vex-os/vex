#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/blk_ata.h>
#include <vex/blk_atapi.h>
#include <vex/blk_device.h>
#include <vex/blk_event.h>
#include <vex/blk_name.h>
#include <vex/clocksrc.h>
#include <vex/delay.h>
#include <vex/endian.h>
#include <vex/event.h>
#include <vex/hart.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mutex.h>
#include <vex/page.h>
#include <vex/pci.h>
#include <vex/pdma.h>
#include <vex/sprintf.h>
#include <vex/string.h>

#define HBA_CAP  0x00 // Host Capabilities
#define HBA_GHC  0x04 // Global Host Control
#define HBA_IS   0x08 // Interrupt Status (global)
#define HBA_PI   0x0C // Ports Implemented
#define HBA_CAP2 0x24 // Host Capabilities Extended
#define HBA_BOHC 0x28 // BIOS/OS Handoff Control and Status

#define HBA_CAP_S64A UINT32_C(0x80000000) // Supports 64-bit Addressing

#define HBA_GHC_AE UINT32_C(0x80000000) // AHCI Enable
#define HBA_GHC_IE UINT32_C(0x00000002) // Interrupt Enable

#define HBA_CAP2_BOH UINT32_C(0x00000001) // BIOS/OS Handoff

#define HBA_BOHC_BOS UINT32_C(0x00000001) // BIOS Owned Semaphore
#define HBA_BOHC_OOS UINT32_C(0x00000002) // OS Owned Semaphore
#define HBA_BOHC_BB  UINT32_C(0x00000010) // BIOS Busy

#define HBA_PORT_BASE 0x100 // Offset of port 0 registers
#define HBA_PORT_SIZE 0x80  // Per-port register stride

#define PORT_REG_CLB  0x00 // Command List Base Address
#define PORT_REG_CLBU 0x04 // Command List Base Address Upper 32 bits
#define PORT_REG_FB   0x08 // FIS Base Address
#define PORT_REG_FBU  0x0C // FIS Base Address Upper 32 bits
#define PORT_REG_IS   0x10 // Interrupt Status
#define PORT_REG_IE   0x14 // Interrupt Enable
#define PORT_REG_CMD  0x18 // Command and Status
#define PORT_REG_TFD  0x20 // Task File Data
#define PORT_REG_SIG  0x24 // Signature
#define PORT_REG_SSTS 0x28 // SATA Status (SCR0)
#define PORT_REG_SCTL 0x2C // SATA Control (SCR2)
#define PORT_REG_SERR 0x30 // SATA Error (SCR1)
#define PORT_REG_SACT 0x34 // SATA Active
#define PORT_REG_CI   0x38 // Command Issue

#define PORT_CMD_ST  UINT32_C(0x00000001) // Start
#define PORT_CMD_FRE UINT32_C(0x00000010) // FIS Receive Enable
#define PORT_CMD_FR  UINT32_C(0x00004000) // FIS Receive Running
#define PORT_CMD_CR  UINT32_C(0x00008000) // Command List Running

#define PORT_IS_DHRS UINT32_C(0x00000001) // Device to Host Register FIS
#define PORT_IS_PSS  UINT32_C(0x00000002) // PIO Setup FIS
#define PORT_IS_DSS  UINT32_C(0x00000004) // DMA Setup FIS
#define PORT_IS_SDBS UINT32_C(0x00000008) // Set Device Bits FIS
#define PORT_IS_IFS  UINT32_C(0x08000000) // Interface Fatal Error
#define PORT_IS_HBDS UINT32_C(0x10000000) // Host Bus Data Error
#define PORT_IS_HBFS UINT32_C(0x20000000) // Host Bus Fatal Error
#define PORT_IS_TFES UINT32_C(0x40000000) // Task File Error

#define PORT_IS_DONE   (PORT_IS_DHRS | PORT_IS_PSS | PORT_IS_DSS | PORT_IS_SDBS)
#define PORT_IS_ERRORS (PORT_IS_IFS | PORT_IS_HBDS | PORT_IS_HBFS | PORT_IS_TFES)

#define PORT_SCTL_DET_MASK  UINT32_C(0x0000000F)
#define PORT_SCTL_DET_RESET UINT32_C(0x00000001) // COMRESET

#define PORT_TFD_ERR UINT32_C(0x00000001) // Task File Error
#define PORT_TFD_DRQ UINT32_C(0x00000008) // Data Request
#define PORT_TFD_BSY UINT32_C(0x00000080) // Busy

#define PORT_SSTS_DET_PRESENT 3 // Device detected, PHY communication established
#define PORT_SSTS_IPM_ACTIVE  1 // Interface in active power state

#define CMD_HEADER_A UINT16_C(0x0020) // ATAPI
#define CMD_HEADER_W UINT16_C(0x0040) // Write

#define CMDTABLE_ACMD 0x40 // ATAPI command

#define FIS_H2D_C UINT8_C(0x80) // Command, not device control

#define ATAPI_FEATURES_DMA         UINT8_C(0x01)
#define ATAPI_CMD_READ_CAPACITY_10 0x25

#define DMA_OFFSET_CMDLIST  0x000 // [0x000..0x3FF] command list (32 slots * 32 bytes)
#define DMA_OFFSET_FIS      0x400 // [0x400..0x4FF] received FIS
#define DMA_OFFSET_CMDTABLE 0x500 // [0x500..0x57F] command table (FIS at 0x00, ATAPI CDB at 0x40)
#define DMA_OFFSET_PRDT     0x580 // [0x580..0x77F] PRDT, AHCI_MAX_PRDT entries
#define DMA_OFFSET_DATABUF  0x780 // [0x780..0x97F] identify and capacity data
#define DMA_DATABUF_SIZE    0x200
#define DMA_NUM_PAGES       1

#define AHCI_MAX_PRDT     32
#define AHCI_MAX_TRANSFER 0x10000 // Bytes per command (64 KiB)

#define AHCI_COMMAND_TIMEOUT_NS  UINT64_C(10000000000)
#define AHCI_ENGINE_TIMEOUT_NS   UINT64_C(500000000) // PxCMD.CR and PxCMD.FR, per the spec
#define AHCI_LINK_TIMEOUT_NS     UINT64_C(1000000000)
#define AHCI_BIOS_BOS_TIMEOUT_NS UINT64_C(25000000)
#define AHCI_BIOS_BB_TIMEOUT_NS  UINT64_C(2000000000)

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

static_assert(DMA_OFFSET_PRDT + AHCI_MAX_PRDT * sizeof(struct ahci_prdt_entry) <= DMA_OFFSET_DATABUF);
static_assert(DMA_OFFSET_DATABUF + DMA_DATABUF_SIZE <= DMA_NUM_PAGES * PAGE_SIZE);

struct ahci_pci_context;

struct ahci_port {
    struct mutex lock;   // Held for a whole command, every one of which uses slot 0 and the DMA buffer
    struct event done;   // Triggered by the interrupt handler, reset before each command
    uint32_t irq_status; // PxIS bits the interrupt handler cleared since the command was issued
    struct ahci_pci_context* context;
    struct pci_bar* bar;
    uintptr_t port_base;
    uintptr_t dma_phys;
    uint8_t* dma_virt;
    unsigned iterator;
    unsigned number;
};

struct ahci_pci_context {
    struct pci_bar* abar;
    uintptr_t dma_limit;
    bool irq;
    struct blk_device devices[32];
    struct atapi_device atapi_devices[32];
    struct ahci_port ports[32];
    unsigned atapi_iterator;
    unsigned ata_iterator;
    uint32_t atapi_mask;
    uint32_t ata_mask;
    unsigned refcount;
};

static __always_inline uint32_t port_read(struct ahci_port* restrict port, uint32_t offset)
{
    return pci_bar_read32(port->bar, port->port_base + offset);
}

static __always_inline void port_write(struct ahci_port* restrict port, uint32_t offset, uint32_t value)
{
    pci_bar_write32(port->bar, port->port_base + offset, value);
}

// A port torn down by ahci_remove may still be reached
// through a block device someone holds a reference to
static __nodiscard bool port_acquire(struct ahci_port* restrict port)
{
    mutex_acquire(&port->lock);

    if(port->dma_phys == 0) {
        mutex_release(&port->lock);
        return false;
    }

    return true;
}

static void context_decref(struct ahci_pci_context* restrict context)
{
    if(1 == atomic_fetch_sub(&context->refcount, 1, ATOMIC_ACQ_REL)) {
        for(unsigned port_num = 0; port_num < 32; port_num += 1) {
            mutex_deinit(&context->ports[port_num].lock);
            event_deinit(&context->ports[port_num].done);
        }

        kfree(context);
    }
}

static void ahci_blk_release(const struct blk_device* restrict device)
{
    const struct ahci_port* port = device->ctxdata;
    context_decref(port->context);
}

static void ahci_atapi_release(const struct atapi_device* restrict device)
{
    const struct ahci_port* port = device->base.ctxdata;
    context_decref(port->context);
}

static bool bar_wait(struct pci_bar* restrict bar, uintptr_t offset, uint32_t mask, uint32_t value, uint64_t timeout)
{
    uint64_t deadline = clocksrc_nsec() + timeout;

    while((pci_bar_read32(bar, offset) & mask) != value) {
        if(clocksrc_nsec() >= deadline) {
            return false;
        }

        hart_spin();
    }

    return true;
}

static __always_inline bool port_wait(struct ahci_port* restrict port, uint32_t offset, uint32_t mask, uint32_t value, uint64_t timeout)
{
    return bar_wait(port->bar, port->port_base + offset, mask, value, timeout);
}

static bool ahci_stop(struct ahci_port* restrict port)
{
    port_write(port, PORT_REG_CMD, port_read(port, PORT_REG_CMD) & ~PORT_CMD_ST);

    if(!port_wait(port, PORT_REG_CMD, PORT_CMD_CR, 0, AHCI_ENGINE_TIMEOUT_NS)) {
        return false;
    }

    port_write(port, PORT_REG_CMD, port_read(port, PORT_REG_CMD) & ~PORT_CMD_FRE);

    return port_wait(port, PORT_REG_CMD, PORT_CMD_FR, 0, AHCI_ENGINE_TIMEOUT_NS);
}

static void ahci_start(struct ahci_port* restrict port)
{
    port_write(port, PORT_REG_CMD, port_read(port, PORT_REG_CMD) | PORT_CMD_FRE);
    port_write(port, PORT_REG_CMD, port_read(port, PORT_REG_CMD) | PORT_CMD_ST);
}

static void ahci_comreset(struct ahci_port* restrict port)
{
    uint32_t sctl = port_read(port, PORT_REG_SCTL) & ~PORT_SCTL_DET_MASK;

    port_write(port, PORT_REG_SCTL, sctl | PORT_SCTL_DET_RESET);
    delay_msec(1); // The spec asks for at least 1 ms of COMRESET
    port_write(port, PORT_REG_SCTL, sctl);

    if(!port_wait(port, PORT_REG_SSTS, 0x0F, PORT_SSTS_DET_PRESENT, AHCI_LINK_TIMEOUT_NS)) {
        pr_verbose("ahci: %u.%u: link did not come back after COMRESET", port->iterator, port->number);
    }
}

static void ahci_recover(struct ahci_port* restrict port)
{
    if(!ahci_stop(port)) {
        pr_verbose("ahci: %u.%u: command engine did not stop", port->iterator, port->number);
    }

    if(port_read(port, PORT_REG_TFD) & (PORT_TFD_BSY | PORT_TFD_DRQ)) {
        ahci_comreset(port);
    }

    port_write(port, PORT_REG_SERR, 0xFFFFFFFF);
    port_write(port, PORT_REG_IS, 0xFFFFFFFF);

    ahci_start(port);
}

static int ahci_wait(struct ahci_port* restrict port, uintptr_t lba)
{
    uint64_t deadline = clocksrc_nsec() + AHCI_COMMAND_TIMEOUT_NS;

    // Nothing can switch away from a caller with interrupts
    // disabled, such as kmain mounting the root during boot
    bool sleep = port->context->irq && interrupt_state();

    for(;;) {
        uint32_t status = atomic_load(&port->irq_status, ATOMIC_ACQUIRE) | port_read(port, PORT_REG_IS);

        if((status & PORT_IS_ERRORS) || (port_read(port, PORT_REG_TFD) & PORT_TFD_ERR)) {
            unsigned long tfd = port_read(port, PORT_REG_TFD);
            pr_verbose("ahci: %u.%u: command error: lba=%zu is=%08lX tfd=%08lX", port->iterator, port->number, (size_t)(lba),
                (unsigned long)(status), tfd);
            return -EIO;
        }

        if(0 == (port_read(port, PORT_REG_CI) & 1)) {
            return 0;
        }

        uint64_t now = clocksrc_nsec();

        if(now >= deadline) {
            pr_verbose("ahci: %u.%u: command timeout: lba=%zu", port->iterator, port->number, (size_t)(lba));
            return -EIO;
        }

        if(sleep) {
            struct event* done = &port->done;
            ssize_t result = event_await_timeout(&done, 1, deadline - now);
            (void)(result); // Completion is read from the registers either way
        }
        else {
            hart_spin();
        }
    }
}

static void ahci_setup_header(struct ahci_port* restrict port, uint16_t flags, const struct blk_segment* restrict segments,
    size_t num_segments)
{
    volatile struct ahci_cmd_header* header = (volatile struct ahci_cmd_header*)(port->dma_virt + DMA_OFFSET_CMDLIST);
    volatile struct ahci_prdt_entry* prdt = (volatile struct ahci_prdt_entry*)(port->dma_virt + DMA_OFFSET_PRDT);

    uintptr_t cmdtable_phys = port->dma_phys + DMA_OFFSET_CMDTABLE;

    kassert(num_segments <= AHCI_MAX_PRDT);

    header->flags = htole16(flags | (sizeof(struct ata_fis_h2d) / sizeof(uint32_t))); // CFL, in dwords
    header->prdtl = htole16((uint16_t)(num_segments));
    header->prdbc = 0;
    header->ctba = htole32(cmdtable_phys);
    header->ctbau = htole32(cmdtable_phys >> 32);
    header->reserved[0] = 0;
    header->reserved[1] = 0;
    header->reserved[2] = 0;
    header->reserved[3] = 0;

    for(size_t i = 0; i < num_segments; ++i) {
        prdt[i].dba = htole32(segments[i].phys);
        prdt[i].dbau = htole32(segments[i].phys >> 32);
        prdt[i].reserved = 0;
        prdt[i].dbc = htole32((uint32_t)(segments[i].length) - 1);
    }
}

static void ahci_setup(struct ahci_port* restrict port, uint64_t lba, uint8_t cmd, uint16_t count,
    const struct blk_segment* restrict segments, size_t num_segments, bool write)
{
    volatile struct ata_fis_h2d* fis = (volatile struct ata_fis_h2d*)(port->dma_virt + DMA_OFFSET_CMDTABLE);

    memset((void*)fis, 0, sizeof(struct ata_fis_h2d));

    ahci_setup_header(port, write ? CMD_HEADER_W : 0, segments, num_segments);

    fis->type = ATA_FIS_TYPE_H2D;
    fis->pm_port = FIS_H2D_C;
    fis->command = cmd;
    fis->device = ATA_DEVICE_LBA;
    fis->lba0 = lba >> 0;
    fis->lba1 = lba >> 8;
    fis->lba2 = lba >> 16;
    fis->lba3 = lba >> 24;
    fis->lba4 = lba >> 32;
    fis->lba5 = lba >> 40;
    fis->count_lo = count >> 0;
    fis->count_hi = count >> 8;
}

static int ahci_issue(struct ahci_port* restrict port, uintptr_t lba)
{
    port_write(port, PORT_REG_SERR, 0xFFFFFFFF);
    port_write(port, PORT_REG_IS, 0xFFFFFFFF);

    if(!port_wait(port, PORT_REG_TFD, PORT_TFD_BSY | PORT_TFD_DRQ, 0, AHCI_COMMAND_TIMEOUT_NS)) {
        pr_verbose("ahci: %u.%u: device stuck busy: lba=%zu", port->iterator, port->number, (size_t)(lba));
        ahci_recover(port);
        return -EIO;
    }

    atomic_store(&port->irq_status, 0, ATOMIC_RELEASE);
    event_reset(&port->done);

    port_write(port, PORT_REG_CI, 1);

    int retval = ahci_wait(port, lba);

    if(retval < 0) {
        ahci_recover(port);
    }

    return retval;
}

static int ahci_identify(struct ahci_port* restrict port, ata_identify_type identify)
{
    volatile uint8_t* databuf = (volatile uint8_t*)(port->dma_virt + DMA_OFFSET_DATABUF);

    struct blk_segment segment = { port->dma_phys + DMA_OFFSET_DATABUF, sizeof(ata_identify_type) };

    memset((void*)databuf, 0, sizeof(ata_identify_type));

    ahci_setup(port, 0, ATA_CMD_IDENTIFY, 0, &segment, 1, false);

    int retval = ahci_issue(port, 0);

    if(retval < 0) {
        return retval;
    }

    memcpy(identify, (const void*)(databuf), sizeof(ata_identify_type));

    return 0;
}

static int ahci_atapi_identify(struct ahci_port* restrict port, ata_identify_type identify)
{
    volatile uint8_t* databuf = (volatile uint8_t*)(port->dma_virt + DMA_OFFSET_DATABUF);

    struct blk_segment segment = { port->dma_phys + DMA_OFFSET_DATABUF, sizeof(ata_identify_type) };

    memset((void*)databuf, 0, sizeof(ata_identify_type));

    ahci_setup(port, 0, ATA_CMD_IDENTIFY_ATAPI, 0, &segment, 1, false);

    int retval = ahci_issue(port, 0);

    if(retval < 0) {
        return retval;
    }

    memcpy(identify, (const void*)(databuf), sizeof(ata_identify_type));

    return 0;
}

static void ahci_atapi_setup(struct ahci_port* restrict port, const atapi_cdb_type cdb, const struct blk_segment* restrict segments,
    size_t num_segments, size_t byte_count, bool write)
{
    volatile struct ata_fis_h2d* fis = (volatile struct ata_fis_h2d*)(port->dma_virt + DMA_OFFSET_CMDTABLE);
    volatile uint8_t* atapi_area = port->dma_virt + DMA_OFFSET_CMDTABLE + CMDTABLE_ACMD;

    memset((void*)fis, 0, sizeof(struct ata_fis_h2d));

    ahci_setup_header(port, (write ? CMD_HEADER_W : 0) | CMD_HEADER_A, segments, num_segments);

    fis->type = ATA_FIS_TYPE_H2D;
    fis->pm_port = FIS_H2D_C;
    fis->command = ATA_CMD_PACKET;
    fis->features = ATAPI_FEATURES_DMA;
    fis->lba1 = byte_count & 0xFF;
    fis->lba2 = (byte_count >> 8) & 0xFF;

    memcpy((void*)atapi_area, cdb, sizeof(atapi_cdb_type));
}

static int ahci_atapi_capacity(struct ahci_port* restrict port, size_t* restrict sector_count, size_t* restrict sector_size)
{
    volatile uint8_t* databuf = (volatile uint8_t*)(port->dma_virt + DMA_OFFSET_DATABUF);

    atapi_cdb_type cdb;
    memset(cdb, 0, sizeof(atapi_cdb_type));
    cdb[0] = ATAPI_CMD_READ_CAPACITY_10;

    uint8_t result[8];
    struct blk_segment segment = { port->dma_phys + DMA_OFFSET_DATABUF, sizeof(result) };

    memset((void*)databuf, 0, sizeof(result));

    ahci_atapi_setup(port, cdb, &segment, 1, sizeof(result), false);

    int retval = ahci_issue(port, 0);

    if(retval < 0) {
        return retval;
    }

    memcpy(result, (const void*)databuf, sizeof(result));

    *sector_count = (size_t)result[0] << 24;
    *sector_count |= (size_t)result[1] << 16;
    *sector_count |= (size_t)result[2] << 8;
    *sector_count |= (size_t)result[3];
    *sector_count += 1;

    *sector_size = (size_t)result[4] << 24;
    *sector_size |= (size_t)result[5] << 16;
    *sector_size |= (size_t)result[6] << 8;
    *sector_size |= (size_t)result[7];

    return 0;
}

static void finish(struct blk_request* restrict req, int status)
{
    req->status = status;
    req->done(req);
}

static void ahci_atapi_submit(const struct atapi_device* restrict device, const atapi_cdb_type cdb, struct blk_request* restrict req)
{
    struct ahci_port* port = device->base.ctxdata;

    if(!port_acquire(port)) {
        finish(req, -ENODEV);
        return;
    }

    ahci_atapi_setup(port, cdb, req->segments, req->num_segments, req->length, req->op == BLK_REQ_WRITE);

    int retval = ahci_issue(port, req->lba);

    mutex_release(&port->lock);

    finish(req, retval);
}

static const struct atapi_device_ops ahci_atapi_ops = {
    .submit_packet = &ahci_atapi_submit,
    .release = &ahci_atapi_release,
};

static void ahci_submit(const struct blk_device* restrict device, struct blk_request* restrict req)
{
    struct ahci_port* port = device->ctxdata;
    uint16_t count = (uint16_t)(req->length / device->sector_size);

    if(!port_acquire(port)) {
        finish(req, -ENODEV);
        return;
    }

    switch(req->op) {
        case BLK_REQ_READ:
            ahci_setup(port, req->lba, ATA_CMD_READ_DMA_EXT, count, req->segments, req->num_segments, false);
            break;

        case BLK_REQ_WRITE:
            ahci_setup(port, req->lba, ATA_CMD_WRITE_DMA_EXT, count, req->segments, req->num_segments, true);
            break;

        default:
            ahci_setup(port, 0, ATA_CMD_FLUSH_EXT, 0, nullptr, 0, false);
            break;
    }

    int retval = ahci_issue(port, req->lba);

    mutex_release(&port->lock);

    finish(req, retval);
}

static int ahci_port_init(struct ahci_pci_context* restrict context, struct pci_bar* restrict abar, unsigned port_num)
{
    struct ahci_port* port = &context->ports[port_num];

    uintptr_t port_base = HBA_PORT_BASE + port_num * HBA_PORT_SIZE;
    uintptr_t dma_phys = pdma_alloc_phys(DMA_NUM_PAGES);

    if(dma_phys == 0) {
        pr_verbose("ahci: out of memory");
        return -ENOMEM;
    }

    uint8_t* dma_virt = phys_to_virt(dma_phys);

    memset(dma_virt, 0, PAGE_SIZE * DMA_NUM_PAGES);

    mutex_init(&port->lock);
    event_init(&port->done);
    port->irq_status = 0;
    port->number = port_num;
    port->bar = abar;
    port->port_base = port_base;
    port->dma_phys = dma_phys;
    port->dma_virt = dma_virt;

    if(!ahci_stop(port)) {
        pr_verbose("ahci: %u: command engine did not stop", port_num);
        pdma_free_phys(dma_phys, DMA_NUM_PAGES);
        port->dma_phys = 0;
        return -EIO;
    }

    pci_bar_write32(abar, port_base + PORT_REG_CLB, dma_phys + DMA_OFFSET_CMDLIST);
    pci_bar_write32(abar, port_base + PORT_REG_CLBU, (dma_phys + DMA_OFFSET_CMDLIST) >> 32);
    pci_bar_write32(abar, port_base + PORT_REG_FB, dma_phys + DMA_OFFSET_FIS);
    pci_bar_write32(abar, port_base + PORT_REG_FBU, (dma_phys + DMA_OFFSET_FIS) >> 32);

    pci_bar_write32(abar, port_base + PORT_REG_SERR, 0xFFFFFFFF);
    pci_bar_write32(abar, port_base + PORT_REG_IS, 0xFFFFFFFF);
    pci_bar_write32(abar, port_base + PORT_REG_IE, PORT_IS_DONE | PORT_IS_ERRORS);

    ahci_start(port);

    return 0;
}

static void ahci_port_teardown(struct ahci_port* restrict port)
{
    mutex_acquire(&port->lock);

    port_write(port, PORT_REG_IE, 0);

    if(!ahci_stop(port)) {
        pr_verbose("ahci: %u.%u: command engine did not stop", port->iterator, port->number);
    }

    pdma_free_phys(port->dma_phys, DMA_NUM_PAGES);

    port->dma_phys = 0;

    mutex_release(&port->lock);
}

static const struct blk_device_ops ahci_blk_device_ops = {
    .submit = &ahci_submit,
    .release = &ahci_blk_release,
};

static void set_limits(const struct ahci_pci_context* restrict context, struct blk_device* restrict device)
{
    device->max_transfer = AHCI_MAX_TRANSFER;
    device->max_segments = AHCI_MAX_PRDT;
    device->dma_limit = context->dma_limit;
    device->dma_align = 1; // PRDT DBA and DBC both have to be even
}

static int ahci_port_register_ata(struct ahci_pci_context* restrict context, unsigned port_num)
{
    if(context->ata_iterator == UINT_MAX) {
        context->ata_iterator = blk_name_increment(blk_name_ata);
    }

    struct blk_device* device = &context->devices[port_num];
    struct ahci_port* port = &context->ports[port_num];

    port->context = context;

    device->ops = &ahci_blk_device_ops;
    device->sector_count = 0;
    device->sector_size = 0;
    device->flags = 0;
    device->ctxdata = port;

    set_limits(context, device);

    int retval = add_blk_device_root(device, blk_name_ata, context->ata_iterator, port_num);

    if(retval == 0) {
        context->refcount += 1;
    }

    return retval;
}

static int ahci_port_register_atapi(struct ahci_pci_context* restrict context, unsigned port_num)
{
    if(context->atapi_iterator == UINT_MAX) {
        context->atapi_iterator = blk_name_increment(blk_name_atapi);
    }

    struct atapi_device* atapi_dev = &context->atapi_devices[port_num];
    struct ahci_port* port = &context->ports[port_num];

    port->context = context;

    atapi_dev->ops = &ahci_atapi_ops;
    atapi_dev->flags = 0;
    atapi_dev->base.ctxdata = port;
    atapi_dev->base.sector_count = 0;
    atapi_dev->base.sector_size = 0;
    atapi_dev->base.flags = 0;

    set_limits(context, &atapi_dev->base);

    int retval = add_atapi_device(atapi_dev, blk_name_atapi, context->atapi_iterator, port_num);

    if(retval == 0) {
        context->refcount += 1;
    }

    return retval;
}

static void ahci_port_probe_ata(struct ahci_pci_context* restrict context, struct pci_bar* restrict abar, unsigned port_num)
{
    if(ahci_port_init(context, abar, port_num)) {
        return;
    }

    ata_identify_type identify;
    struct ahci_port* port = &context->ports[port_num];

    port->iterator = context->ata_iterator;

    if(ahci_identify(port, identify)) {
        pr_verbose("ahci: %u.%u: identify failed", context->ata_iterator, port_num);
        ahci_port_teardown(port);
        return;
    }

    if(ata_identify_is_atapi(identify)) {
        ahci_port_teardown(port);
        return;
    }

    struct blk_device* device = &context->devices[port_num];
    device->sector_count = ata_identify_sector_count(identify);
    device->sector_size = ata_identify_logical_sector_size(identify);

    if(device->sector_size == 0 || device->sector_size > AHCI_MAX_TRANSFER) {
        pr_verbose("ahci: %u.%u: unsupported sector size %zu", context->ata_iterator, port_num, device->sector_size);
        ahci_port_teardown(port);
        return;
    }

    device->flags = BLK_FLAG_PRESENT;
    device->ctxdata = port;

    char model[41];
    ata_identify_string(&identify[ATA_IDENTIFY_MODEL_FIELD], ATA_IDENTIFY_MODEL_NWORDS, model, sizeof(model));
    pr_verbose("ahci: %u.%u: model=%s sector_count=%zu sector_size=%zu", context->ata_iterator, port_num, model, device->sector_count,
        device->sector_size);

    blk_event_notify(BLK_EVENT_MEDIA_CONNECTED, device);
}

static void ahci_port_probe_atapi(struct ahci_pci_context* restrict context, struct pci_bar* restrict abar, unsigned port_num)
{
    if(ahci_port_init(context, abar, port_num)) {
        return;
    }

    ata_identify_type identify;
    struct ahci_port* port = &context->ports[port_num];
    struct atapi_device* atapi_dev = &context->atapi_devices[port_num];

    port->iterator = context->atapi_iterator;

    char model[41];

    if(0 == ahci_atapi_identify(port, identify)) {
        ata_identify_string(&identify[ATA_IDENTIFY_MODEL_FIELD], ATA_IDENTIFY_MODEL_NWORDS, model, sizeof(model));
    }
    else {
        model[0] = 0x00;
    }

    size_t sector_count;
    size_t sector_size;

    if(ahci_atapi_capacity(port, &sector_count, &sector_size)) {
        pr_verbose("ahci: %u.%u: no media present", context->atapi_iterator, port_num);
        ahci_port_teardown(port);
        return;
    }

    if(sector_size == 0 || sector_size > AHCI_MAX_TRANSFER) {
        pr_verbose("ahci: %u.%u: unsupported sector size %zu", context->atapi_iterator, port_num, sector_size);
        ahci_port_teardown(port);
        return;
    }

    atapi_dev->base.sector_count = sector_count;
    atapi_dev->base.sector_size = sector_size;
    atapi_dev->base.flags = BLK_FLAG_PRESENT;
    atapi_dev->base.ctxdata = port;

    pr_verbose("ahci: %u.%u: model=%s sector_count=%zu sector_size=%zu", context->atapi_iterator, port_num, model, sector_count,
        sector_size);

    blk_event_notify(BLK_EVENT_MEDIA_CONNECTED, &atapi_dev->base);
}

static void ahci_ireq(unsigned number, const struct hart_context* restrict ctx, void* restrict ctxdata)
{
    struct ahci_pci_context* context = ctxdata;
    uint32_t pending = pci_bar_read32(context->abar, HBA_IS);

    for(unsigned port_num = 0; port_num < 32; port_num += 1) {
        if(0 == (pending & (UINT32_C(1) << port_num))) {
            continue;
        }

        struct ahci_port* port = &context->ports[port_num];
        uintptr_t port_base = HBA_PORT_BASE + port_num * HBA_PORT_SIZE;

        // PxIS goes before IS, or the port raises IS again right away
        uint32_t status = pci_bar_read32(context->abar, port_base + PORT_REG_IS);
        pci_bar_write32(context->abar, port_base + PORT_REG_IS, status);

        atomic_fetch_or(&port->irq_status, status, ATOMIC_RELEASE);
        event_trigger(&port->done, true);
    }

    pci_bar_write32(context->abar, HBA_IS, pending);
}

// Firmware that drives the HBA itself (SMI-based legacy
// emulation and the like) has to let go of it first
static void ahci_handoff(struct pci_bar* restrict abar)
{
    if(0 == (pci_bar_read32(abar, HBA_CAP2) & HBA_CAP2_BOH)) {
        return;
    }

    pci_bar_write32(abar, HBA_BOHC, pci_bar_read32(abar, HBA_BOHC) | HBA_BOHC_OOS);

    uint64_t deadline = clocksrc_nsec() + AHCI_BIOS_BOS_TIMEOUT_NS;
    bool busy = false;

    while((pci_bar_read32(abar, HBA_BOHC) & HBA_BOHC_BOS) && clocksrc_nsec() < deadline) {
        if(pci_bar_read32(abar, HBA_BOHC) & HBA_BOHC_BB) {
            busy = true;
            break;
        }

        hart_spin();
    }

    // Firmware that needs longer sets BB within 25 ms and then gets 2 s
    if(busy) {
        (void)bar_wait(abar, HBA_BOHC, HBA_BOHC_BOS, 0, AHCI_BIOS_BB_TIMEOUT_NS);
    }

    if(pci_bar_read32(abar, HBA_BOHC) & HBA_BOHC_BOS) {
        pr_warning("ahci: firmware did not release the HBA, taking it anyway");
    }
}

static int ahci_probe(struct pci_device* restrict pci_dev, const struct pci_driver_id* restrict id)
{
    struct ahci_pci_context* context = kmalloc(sizeof(struct ahci_pci_context));

    if(context == nullptr) {
        pr_verbose("ahci: out of memory");
        return -ENOMEM;
    }

    context->irq = false;
    context->ata_iterator = UINT_MAX;
    context->atapi_iterator = UINT_MAX;
    context->refcount = 1;

    pci_dev->ctxdata = context;

    pci_enable_busmaster(pci_dev);

    struct pci_bar* abar = pci_bar_map(pci_dev, 5);

    if(abar == nullptr) {
        pr_verbose("ahci: pci_bar_map failed");

        kfree(context);

        pci_dev->ctxdata = nullptr;

        return -ENOMEM;
    }

    context->abar = abar;
    context->dma_limit = UINT32_MAX;

    if(pci_bar_read32(abar, HBA_CAP) & HBA_CAP_S64A) {
        context->dma_limit = UINTPTR_MAX;
    }

    ahci_handoff(abar);

    uint32_t ahci_ghc = pci_bar_read32(abar, HBA_GHC);

    if(0 == (ahci_ghc & HBA_GHC_AE)) {
        pci_bar_write32(abar, HBA_GHC, ahci_ghc | HBA_GHC_AE);
    }

    uint32_t ahci_pi = pci_bar_read32(abar, HBA_PI);

    for(unsigned port_num = 0; port_num < 32; port_num += 1) {
        if(0 == (ahci_pi & (UINT32_C(1) << port_num))) {
            continue;
        }

        uintptr_t port_base = HBA_PORT_BASE + port_num * HBA_PORT_SIZE;
        uint32_t ahci_signature = pci_bar_read32(abar, port_base + PORT_REG_SIG);

        if(ahci_signature == ATA_SIGNATURE_ATAPI) {
            if(0 == ahci_port_register_atapi(context, port_num)) {
                context->atapi_mask |= (UINT32_C(1) << port_num);
            }
        }
        else {
            if(0 == ahci_port_register_ata(context, port_num)) {
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

    if(0 == pci_request_irq(pci_dev, &ahci_ireq, context)) {
        context->irq = true;
        pci_bar_write32(abar, HBA_GHC, pci_bar_read32(abar, HBA_GHC) | HBA_GHC_IE);
    }
    else {
        pr_verbose("ahci: no MSI, commands are polled");
    }

    for(unsigned port_num = 0; port_num < 32; port_num += 1) {
        uint32_t port_bit = UINT32_C(1) << port_num;

        if(0 == ((context->ata_mask | context->atapi_mask) & port_bit)) {
            continue;
        }

        uintptr_t port_base = HBA_PORT_BASE + port_num * HBA_PORT_SIZE;
        uint32_t ahci_ssts = pci_bar_read32(abar, port_base + PORT_REG_SSTS);

        if((ahci_ssts & 0x0F) != PORT_SSTS_DET_PRESENT) {
            continue;
        }

        if(((ahci_ssts >> 8) & 0x0F) != PORT_SSTS_IPM_ACTIVE) {
            continue;
        }

        if(context->atapi_mask & port_bit) {
            ahci_port_probe_atapi(context, abar, port_num);
        }
        else {
            ahci_port_probe_ata(context, abar, port_num);
        }
    }

    return 0;
}

static void ahci_remove(struct pci_device* restrict pci_dev)
{
    struct ahci_pci_context* context = pci_dev->ctxdata;

    if(context == nullptr) {
        return;
    }

    if(context->irq) {
        pci_bar_write32(context->abar, HBA_GHC, pci_bar_read32(context->abar, HBA_GHC) & ~HBA_GHC_IE);
        pci_release_irq(pci_dev);
        context->irq = false;
    }

    for(unsigned port_num = 0; port_num < 32; port_num += 1) {
        uint32_t port_bit = UINT32_C(1) << port_num;

        if(context->ata_mask & port_bit) {
            remove_blk_device(&context->devices[port_num]);
        }
        else if(context->atapi_mask & port_bit) {
            remove_atapi_device(&context->atapi_devices[port_num]);
        }
        else {
            continue;
        }

        if(context->ports[port_num].dma_phys) {
            ahci_port_teardown(&context->ports[port_num]);
        }
    }

    pci_bar_unmap(pci_dev, 5);

    pci_dev->ctxdata = nullptr;

    context_decref(context);
}

static const struct pci_driver_ops ahci_driver_ops = {
    .probe = &ahci_probe,
    .remove = &ahci_remove,
};

static const struct pci_driver_id ahci_ids[] = {
    PCI_DEVICE_CLASS(0x01, 0x06),
    PCI_DEVICE_END(),
};

static struct pci_driver ahci_driver = {
    .ops = &ahci_driver_ops,
    .ids = ahci_ids,
};

static void init_ahci(void)
{
    add_pci_driver(&ahci_driver);
}

initcall_bsp_dev(init_ahci, INIT_MIDDLE);
