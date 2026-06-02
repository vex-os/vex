#include <arch/hart.h>
#include <bus/isa.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <dev/ata.h>
#include <dev/block_device.h>
#include <dev/block_namespace.h>
#include <init/initcall.h>
#include <mm/malloc.h>
#include <time/delay.h>

#define IDE_CMD_REG_DATA     0x00 ///< [RO] data
#define IDE_CMD_REG_ERROR    0x01 ///< [RO] error
#define IDE_CMD_REG_FEATURES 0x01 ///< [WO] features
#define IDE_CMD_REG_SECNT0   0x02 ///< [RW] sector count
#define IDE_CMD_REG_LBA0     0x03 ///< [RW] LBA low
#define IDE_CMD_REG_LBA1     0x04 ///< [RW] LBA middle
#define IDE_CMD_REG_LBA2     0x05 ///< [RW] LBA high
#define IDE_CMD_REG_DEVICE   0x06 ///< [RW] device/head
#define IDE_CMD_REG_STATUS   0x07 ///< [RO] status
#define IDE_CMD_REG_COMMAND  0x07 ///< [WO] command

#define IDE_CTRL_REG_ALTSTATUS 0x00 ///< [RO] alternate status
#define IDE_CTRL_REG_CONTROL   0x00 ///< [WO] control

#define IDE_CHANNEL_SLAVE (1U << 0) ///< Set if this IDE channel is a slave (secondary) channel
#define IDE_CHANNEL_LBA48 (1U << 1) ///< Set if this IDE channel supports 48-bit LBA addressing

#define IDE_DELAY_NS 400

#define IDE_MASTER_CMD_BASE  0x1F0
#define IDE_MASTER_CTRL_BASE 0x3F6
#define IDE_SLAVE_CMD_BASE   0x170
#define IDE_SLAVE_CTRL_BASE  0x376

struct ide_channel {
    struct isa_device* device;
    struct pmio_region* io_cmd;
    struct pmio_region* io_ctrl;
    size_t sector_count;
    size_t sector_size;
    unsigned flags;
};

static int ide_wait_bsy(struct ide_channel* restrict ide)
{
    unsigned long i;

    for(i = 0; i < 0x100000; ++i) {
        if(0 == (pmio_read8(ide->io_ctrl, IDE_CTRL_REG_ALTSTATUS) & ATA_STATUS_BSY)) {
            return 0;
        }
    }

    return EIO;
}

static int ide_wait_drq(struct ide_channel* restrict ide)
{
    uint8_t status;
    unsigned long i;

    for(i = 0; i < 0x100000; ++i) {
        status = pmio_read8(ide->io_ctrl, IDE_CTRL_REG_ALTSTATUS);

        if((status & ATA_STATUS_ERR) || (status & ATA_STATUS_DF)) {
            return EIO;
        }

        if(status & ATA_STATUS_DRQ) {
            return 0;
        }
    }

    return EIO;
}

static void ide_select(struct ide_channel* restrict ide)
{
    uint8_t device = ATA_DEVICE_FIXED | ATA_DEVICE_LBA;

    if(ide->flags & IDE_CHANNEL_SLAVE) {
        device |= ATA_DEVICE_SLAVE;
    }

    pmio_write8(ide->io_cmd, IDE_CMD_REG_DEVICE, device);
    delay_nsec(IDE_DELAY_NS);
}

static void ide_setup_lba(struct ide_channel* restrict ch, uint64_t lba, uint8_t count)
{
    uint8_t dev = ATA_DEVICE_FIXED | ATA_DEVICE_LBA;

    if(ch->flags & IDE_CHANNEL_SLAVE) {
        dev |= ATA_DEVICE_SLAVE;
    }

    if(ch->flags & IDE_CHANNEL_LBA48) {
        pmio_write8(ch->io_cmd, IDE_CMD_REG_SECNT0, 0);
        pmio_write8(ch->io_cmd, IDE_CMD_REG_LBA0, (uint8_t)(lba >> 24));
        pmio_write8(ch->io_cmd, IDE_CMD_REG_LBA1, (uint8_t)(lba >> 32));
        pmio_write8(ch->io_cmd, IDE_CMD_REG_LBA2, (uint8_t)(lba >> 40));
    }

    pmio_write8(ch->io_cmd, IDE_CMD_REG_SECNT0, count);
    pmio_write8(ch->io_cmd, IDE_CMD_REG_LBA0, (uint8_t)(lba >> 0));
    pmio_write8(ch->io_cmd, IDE_CMD_REG_LBA1, (uint8_t)(lba >> 8));
    pmio_write8(ch->io_cmd, IDE_CMD_REG_LBA2, (uint8_t)(lba >> 16));
    pmio_write8(ch->io_cmd, IDE_CMD_REG_DEVICE, dev);
}

static int ide_op_read(struct block_device* restrict device, uintptr_t lba, size_t count, void* restrict buffer)
{
    struct ide_channel* ide = device->ctxdata;

    uint8_t* buffer_ptr;
    unsigned status;
    uint8_t command;
    size_t sector;
    int retval;

    if(ide->flags & IDE_CHANNEL_LBA48) {
        command = ATA_CMD_READ_PIO_EXT;
    }
    else {
        command = ATA_CMD_READ_PIO;
    }

    for(sector = 0; sector < count; sector += 1) {
        retval = ide_wait_bsy(ide);

        if(retval) {
            pr_warning("ide: ata bsy timeout on read: lba=%zu", (size_t)(lba + sector));
            return retval;
        }

        ide_setup_lba(ide, lba + sector, 1);
        pmio_write8(ide->io_cmd, IDE_CMD_REG_COMMAND, command);
        delay_nsec(IDE_DELAY_NS);

        retval = ide_wait_drq(ide);

        if(retval) {
            status = pmio_read8(ide->io_ctrl, IDE_CTRL_REG_ALTSTATUS);
            pr_warning("ide: ata drq error on read: lba=%zu status=%02X", (size_t)(lba + sector), status);
            return retval;
        }

        buffer_ptr = buffer;
        buffer_ptr += sector * ide->sector_size;
        pmio_read16_string(ide->io_cmd, IDE_CMD_REG_DATA, (uint16_t*)(buffer_ptr), ide->sector_size / 2);
    }

    return 0;
}

static int ide_op_write(struct block_device* restrict device, uintptr_t lba, size_t count, const void* restrict buffer)
{
    struct ide_channel* ide = device->ctxdata;

    const uint8_t* buffer_ptr;
    unsigned status;
    uint8_t command;
    uint8_t flush;
    size_t sector;
    int retval;

    if(ide->flags & IDE_CHANNEL_LBA48) {
        command = ATA_CMD_WRITE_PIO_EXT;
        flush = ATA_CMD_FLUSH_EXT;
    }
    else {
        command = ATA_CMD_WRITE_PIO;
        flush = ATA_CMD_FLUSH;
    }

    for(sector = 0; sector < count; sector += 1) {
        retval = ide_wait_bsy(ide);

        if(retval) {
            pr_warning("ide: bsy timeout before pio write: lba=%zu", (size_t)(lba + sector));
            return retval;
        }

        ide_setup_lba(ide, lba + sector, 1);
        pmio_write8(ide->io_cmd, IDE_CMD_REG_COMMAND, command);
        delay_nsec(IDE_DELAY_NS);

        retval = ide_wait_drq(ide);

        if(retval) {
            status = pmio_read8(ide->io_ctrl, IDE_CTRL_REG_ALTSTATUS);
            pr_warning("ide: drq error on pio write: lba=%zu status=%02X", (size_t)(lba + sector), status);
            return retval;
        }

        buffer_ptr = buffer;
        buffer_ptr += sector * ide->sector_size;
        pmio_write16_string(ide->io_cmd, IDE_CMD_REG_DATA, (const uint16_t*)(buffer_ptr), ide->sector_size / 2);

        pmio_write8(ide->io_cmd, IDE_CMD_REG_COMMAND, flush);

        retval = ide_wait_bsy(ide);

        if(retval) {
            pr_warning("ide: bsy timeout after pio flush: lba=%zu", (size_t)(lba + sector));
            return retval;
        }
    }

    return 0;
}

static int ide_op_flush(struct block_device* restrict device)
{
    struct ide_channel* ide = device->ctxdata;
    uint8_t command;

    if(ide->flags & IDE_CHANNEL_LBA48) {
        command = ATA_CMD_FLUSH_EXT;
    }
    else {
        command = ATA_CMD_FLUSH;
    }

    pmio_write8(ide->io_cmd, IDE_CMD_REG_COMMAND, command);

    return ide_wait_bsy(ide);
}

static const struct block_device_ops ide_block_device_ops = {
    .read = &ide_op_read,
    .write = &ide_op_write,
    .flush = &ide_op_flush,
    .destroy = nullptr,
};

static int ide_identify(struct ide_channel* restrict ide)
{
    ata_identify_type identify;
    uint8_t status;
    char model[41];

    ide_select(ide);

    pmio_write8(ide->io_cmd, IDE_CMD_REG_SECNT0, 0);
    pmio_write8(ide->io_cmd, IDE_CMD_REG_LBA0, 0);
    pmio_write8(ide->io_cmd, IDE_CMD_REG_LBA1, 0);
    pmio_write8(ide->io_cmd, IDE_CMD_REG_LBA2, 0);
    pmio_write8(ide->io_cmd, IDE_CMD_REG_COMMAND, ATA_CMD_IDENTIFY);

    ide_do_delay();

    status = pmio_read8(ide->io_ctrl, IDE_CTRL_REG_ALTSTATUS);

    if(status == 0) {
        return ENODEV;
    }

    if(ide_wait_bsy(ide)) {
        return EIO;
    }

    if(ide_wait_drq(ide)) {
        return EIO;
    }

    pmio_read16_string(ide->io_cmd, IDE_CMD_REG_DATA, identify, sizeof(identify) / sizeof(uint16_t));

    if(ata_identify_is_atapi(identify)) {
        return ENOTSUP; // ATAPI devices are not supported yet
    }

    if(ata_identify_has_lba48(identify)) {
        ide->flags |= IDE_CHANNEL_LBA48;
    }

    ide->sector_size = ata_identify_logical_sector_size(identify);
    ide->sector_count = ata_identify_sector_count(identify);

    memset(model, 0, sizeof(model));
    ata_identify_string(&identify[ATA_IDENTIFY_MODEL_FIELD], ATA_IDENTIFY_MODEL_NWORDS, model, sizeof(model));

    pr_debug("ide: %s: sector_count=%zu sector_size=%zu", model, ide->sector_count, ide->sector_size);

    return 0;
}

#if 0
static void ide_probe_pair(struct pmio_region io_cmd, struct pmio_region io_ctrl)
{
    // Disable IRQs on this channel while we poll
    pmio_write8(&io_ctrl, ATA_REG_CONTROL, ATA_CONTROL_NIEN);

    ide_probe_channel(io_cmd, io_ctrl, 0); // master
    ide_probe_channel(io_cmd, io_ctrl, 1); // slave
}

static struct pmio_region ide_legacy_region(uintptr_t base)
{
    return (struct pmio_region) { .base = base, .length = 8, .ctxdata = nullptr };
}

static int ide_probe(struct pci_device* restrict pci_dev, const struct pci_driver_id* restrict id)
{
    uint8_t progif = pci_cfg_read8(pci_dev, PCI_CFG_PROG_IF);
    struct pmio_region pri_cmd, pri_ctrl, sec_cmd, sec_ctrl;
    struct pci_bar* bar;

    pci_enable_busmaster(pci_dev);

    // Primary channel
    if(progif & IDE_PROGIF_PRI_NATIVE) {
        bar = pci_bar_map(pci_dev, 0);

        if(bar == nullptr) {
            return ENOMEM;
        }

        pri_cmd = bar->pmio;

        bar = pci_bar_map(pci_dev, 1);

        if(bar == nullptr) {
            return ENOMEM;
        }

        pri_ctrl = bar->pmio;
        pri_ctrl.base += 2; // control register is at offset 2 in the BAR
    }
    else {
        pr_error("LEGACY");
        pri_cmd = ide_legacy_region(IDE_PRI_CMD_BASE);
        pri_ctrl = ide_legacy_region(IDE_PRI_CTRL_BASE);
    }

    // Secondary channel
    if(progif & IDE_PROGIF_SEC_NATIVE) {
        bar = pci_bar_map(pci_dev, 2);

        if(bar == nullptr) {
            return ENOMEM;
        }

        sec_cmd = bar->pmio;

        bar = pci_bar_map(pci_dev, 3);

        if(bar == nullptr) {
            return ENOMEM;
        }

        sec_ctrl = bar->pmio;
        sec_ctrl.base += 2;
    }
    else {
        sec_cmd = ide_legacy_region(IDE_SEC_CMD_BASE);
        sec_ctrl = ide_legacy_region(IDE_SEC_CTRL_BASE);
    }

    ide_probe_pair(pri_cmd, pri_ctrl);
    ide_probe_pair(sec_cmd, sec_ctrl);

    return 0;
}

static void ide_remove(struct pci_device* restrict pci_dev)
{
    // empty
}

static const struct pci_driver_ops ide_driver_ops = {
    .probe = &ide_probe,
    .remove = &ide_remove,
};

static const struct pci_driver_id ide_ids[] = {
    PCI_DEVICE_CLASS(0x01, 0x01), // Mass Storage / IDE
    PCI_DEVICE_END(),
};

static struct pci_driver ide_driver = {
    .ops = &ide_driver_ops,
    .ids = ide_ids,
};

define_initcall(ide, INITCALL_STAGE_BSP_DEV, INITCALL_ORDER_MIDDLE)
{
    pci_driver_register(&ide_driver);
}
#endif
