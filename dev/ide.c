#include <arch/hart.h>
#include <bus/pci.h>
#include <time/delay.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <dev/ata.h>
#include <dev/blkdev.h>
#include <init/initcall.h>
#include <lib/sprintf.h>
#include <lib/string.h>
#include <mm/malloc.h>

// ATA register offsets from CMD base
#define ATA_REG_DATA    0x00 ///< Data (16-bit)
#define ATA_REG_ERROR   0x01 ///< Error (read) / Features (write)
#define ATA_REG_SECNT0  0x02 ///< Sector Count
#define ATA_REG_LBA0    0x03 ///< LBA Low
#define ATA_REG_LBA1    0x04 ///< LBA Mid
#define ATA_REG_LBA2    0x05 ///< LBA High
#define ATA_REG_DEVICE  0x06 ///< Device / Drive select
#define ATA_REG_STATUS  0x07 ///< Status (read) / Command (write)
#define ATA_REG_COMMAND 0x07

// Control register offset from CTRL base
#define ATA_REG_ALTSTATUS 0x00 ///< Alternate Status (read) / Device Control (write)
#define ATA_REG_CONTROL   0x00

// PCI IDE prog-if bits
#define IDE_PROGIF_PRI_NATIVE (1 << 0) ///< Primary channel in native PCI mode
#define IDE_PROGIF_SEC_NATIVE (1 << 2) ///< Secondary channel in native PCI mode

// Legacy ISA-compatible I/O base addresses
#define IDE_PRI_CMD_BASE  0x1F0
#define IDE_PRI_CTRL_BASE 0x3F6
#define IDE_SEC_CMD_BASE  0x170
#define IDE_SEC_CTRL_BASE 0x376

struct ide_channel {
    struct pmio_region cmd;
    struct pmio_region ctrl;
    unsigned slave; // 0 = master, 1 = slave
    bool lba48;
    size_t sector_cnt;
    size_t sector_sz;
};

static unsigned ide_dev_count = 0;

static void ide_400ns(void)
{
    delay_nsec(400);
}

static int ide_wait_bsy(struct ide_channel* restrict ch)
{
    unsigned long i;

    for(i = 0; i < 0x100000; ++i) {
        if(0 == (pmio_read8(&ch->ctrl, ATA_REG_ALTSTATUS) & ATA_STATUS_BSY)) {
            return 0;
        }
    }

    return EIO;
}

static int ide_wait_drq(struct ide_channel* restrict ch)
{
    uint8_t status;
    unsigned long i;

    for(i = 0; i < 0x100000; ++i) {
        status = pmio_read8(&ch->ctrl, ATA_REG_ALTSTATUS);

        if(status & ATA_STATUS_ERR) {
            return EIO;
        }

        if(status & ATA_STATUS_DF) {
            return EIO;
        }

        if(status & ATA_STATUS_DRQ) {
            return 0;
        }
    }

    return EIO;
}

static void ide_select(struct ide_channel* restrict ch)
{
    uint8_t dev = ATA_DEVICE_FIXED | ATA_DEVICE_LBA;

    if(ch->slave) {
        dev |= ATA_DEVICE_SLAVE;
    }

    pmio_write8(&ch->cmd, ATA_REG_DEVICE, dev);
    ide_400ns();
}

static void ide_setup_lba(struct ide_channel* restrict ch, uint64_t lba, uint8_t count)
{
    uint8_t dev = ATA_DEVICE_FIXED | ATA_DEVICE_LBA;

    if(ch->slave) {
        dev |= ATA_DEVICE_SLAVE;
    }

    if(ch->lba48) {
        pmio_write8(&ch->cmd, ATA_REG_SECNT0, 0);
        pmio_write8(&ch->cmd, ATA_REG_LBA0, (uint8_t)(lba >> 24));
        pmio_write8(&ch->cmd, ATA_REG_LBA1, (uint8_t)(lba >> 32));
        pmio_write8(&ch->cmd, ATA_REG_LBA2, (uint8_t)(lba >> 40));
    }

    pmio_write8(&ch->cmd, ATA_REG_SECNT0, count);
    pmio_write8(&ch->cmd, ATA_REG_LBA0, (uint8_t)(lba >> 0));
    pmio_write8(&ch->cmd, ATA_REG_LBA1, (uint8_t)(lba >> 8));
    pmio_write8(&ch->cmd, ATA_REG_LBA2, (uint8_t)(lba >> 16));
    pmio_write8(&ch->cmd, ATA_REG_DEVICE, dev);
}

static int ide_op_read(struct blkdev* restrict device, uintptr_t lba, size_t count, void* restrict buffer)
{
    struct ide_channel* ch = device->ctxdata;
    uint8_t cmd;
    size_t sector;
    int retval;

    cmd = ch->lba48 ? ATA_CMD_READ_PIO_EXT : ATA_CMD_READ_PIO;

    for(sector = 0; sector < count; sector += 1) {
        retval = ide_wait_bsy(ch);

        if(retval) {
            pr_error("ide: BSY timeout before read lba=%llu", (unsigned long long)(lba + sector));
            return retval;
        }

        ide_setup_lba(ch, lba + sector, 1);
        pmio_write8(&ch->cmd, ATA_REG_COMMAND, cmd);

        ide_400ns();

        retval = ide_wait_drq(ch);

        if(retval) {
            pr_error("ide: DRQ error on read lba=%llu status=0x%02X",
                (unsigned long long)(lba + sector),
                (unsigned)(pmio_read8(&ch->ctrl, ATA_REG_ALTSTATUS)));
            return retval;
        }

        pmio_read16_string(&ch->cmd, ATA_REG_DATA,
            (uint16_t*)((uint8_t*)(buffer) + sector * ch->sector_sz),
            ch->sector_sz / 2);
    }

    return 0;
}

static int ide_op_write(struct blkdev* restrict device, uintptr_t lba, size_t count, const void* restrict buffer)
{
    struct ide_channel* ch = device->ctxdata;
    uint8_t cmd;
    size_t sector;
    int retval;

    cmd = ch->lba48 ? ATA_CMD_WRITE_PIO_EXT : ATA_CMD_WRITE_PIO;

    for(sector = 0; sector < count; sector += 1) {
        retval = ide_wait_bsy(ch);

        if(retval) {
            pr_error("ide: BSY timeout before write lba=%llu", (unsigned long long)(lba + sector));
            return retval;
        }

        ide_setup_lba(ch, lba + sector, 1);
        pmio_write8(&ch->cmd, ATA_REG_COMMAND, cmd);

        ide_400ns();

        retval = ide_wait_drq(ch);

        if(retval) {
            pr_error("ide: DRQ error on write lba=%llu status=0x%02X",
                (unsigned long long)(lba + sector),
                (unsigned)(pmio_read8(&ch->ctrl, ATA_REG_ALTSTATUS)));
            return retval;
        }

        pmio_write16_string(&ch->cmd, ATA_REG_DATA,
            (const uint16_t*)((const uint8_t*)(buffer) + sector * ch->sector_sz),
            ch->sector_sz / 2);

        // flush write buffer to media
        pmio_write8(&ch->cmd, ATA_REG_COMMAND, ch->lba48 ? ATA_CMD_FLUSH_EXT : ATA_CMD_FLUSH);
        retval = ide_wait_bsy(ch);

        if(retval) {
            pr_error("ide: BSY timeout after flush lba=%llu", (unsigned long long)(lba + sector));
            return retval;
        }
    }

    return 0;
}

static int ide_op_flush(struct blkdev* restrict device)
{
    struct ide_channel* ch = device->ctxdata;
    int retval;

    retval = ide_wait_bsy(ch);

    if(retval) {
        return retval;
    }

    ide_select(ch);
    pmio_write8(&ch->cmd, ATA_REG_COMMAND, ch->lba48 ? ATA_CMD_FLUSH_EXT : ATA_CMD_FLUSH);

    return ide_wait_bsy(ch);
}

static const struct blkdev_ops ide_blkdev_ops = {
    .read = &ide_op_read,
    .write = &ide_op_write,
    .flush = &ide_op_flush,
    .destroy = nullptr,
};

static bool ide_identify(struct ide_channel* restrict ch)
{
    ata_identify_type identify;
    uint8_t status;
    char model[41];

    ide_select(ch);

    pmio_write8(&ch->cmd, ATA_REG_SECNT0, 0);
    pmio_write8(&ch->cmd, ATA_REG_LBA0, 0);
    pmio_write8(&ch->cmd, ATA_REG_LBA1, 0);
    pmio_write8(&ch->cmd, ATA_REG_LBA2, 0);
    pmio_write8(&ch->cmd, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    ide_400ns();

    status = pmio_read8(&ch->ctrl, ATA_REG_ALTSTATUS);

    if(status == 0) {
        return false; // no device
    }

    if(ide_wait_bsy(ch) != 0) {
        return false;
    }

    // non-zero LBA1/LBA2 means ATAPI or something else — skip
    if(pmio_read8(&ch->cmd, ATA_REG_LBA1) || pmio_read8(&ch->cmd, ATA_REG_LBA2)) {
        return false;
    }

    if(ide_wait_drq(ch) != 0) {
        return false;
    }

    pmio_read16_string(&ch->cmd, ATA_REG_DATA, identify, 256);

    ch->lba48 = ata_identify_has_lba48(identify);
    ch->sector_sz = ata_identify_logical_sector_size(identify);
    ch->sector_cnt = ch->lba48
        ? ata_identify_lba48_capacity(identify)
        : ata_identify_lba28_capacity(identify);

    ata_identify_string(&identify[27], 20, model, sizeof(model));

    pr_inform("ide: found \"%s\" lba48=%d sectors=%zu sector_sz=%zu",
        model, (int)(ch->lba48), ch->sector_cnt, ch->sector_sz);

    return true;
}

static void ide_probe_channel(struct pmio_region cmd, struct pmio_region ctrl, unsigned slave)
{
    struct ide_channel* ch;
    struct blkdev* device;

    ch = kmalloc(sizeof(struct ide_channel));

    if(ch == nullptr) {
        return;
    }

    memset(ch, 0, sizeof(struct ide_channel));

    ch->cmd = cmd;
    ch->ctrl = ctrl;
    ch->slave = slave;

    if(!ide_identify(ch)) {
        kfree(ch);
        return;
    }

    device = kmalloc(sizeof(struct blkdev));

    if(device == nullptr) {
        kfree(ch);
        return;
    }

    memset(device, 0, sizeof(struct blkdev));

    snprintf(device->identifier, sizeof(device->identifier), "ide:%u", ide_dev_count++);

    device->ops = &ide_blkdev_ops;
    device->sector_cnt = ch->sector_cnt;
    device->sector_sz = ch->sector_sz;
    device->ctxdata = ch;

    if(register_blkdev(device) != 0) {
        kfree(ch);
        kfree(device);
    }
}

static void ide_probe_pair(struct pmio_region cmd, struct pmio_region ctrl)
{
    // Disable IRQs on this channel while we poll
    pmio_write8(&ctrl, ATA_REG_CONTROL, ATA_CONTROL_NIEN);

    ide_probe_channel(cmd, ctrl, 0); // master
    ide_probe_channel(cmd, ctrl, 1); // slave
}

static struct pmio_region ide_legacy_region(uintptr_t base)
{
    return (struct pmio_region){ .base = base, .length = 8, .ctxdata = nullptr };
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
    register_pci_driver(&ide_driver);
}
