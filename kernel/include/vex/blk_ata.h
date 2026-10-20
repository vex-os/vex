#pragma once

#include <vex/compiler.h>
#include <vex/endian.h>
#include <vex/types.h>

#define ATA_CMD_READ_PIO       0x20 // [28-bit LBA] Read Sectors
#define ATA_CMD_READ_PIO_EXT   0x24 // [48-bit LBA] Read Sectors Ext
#define ATA_CMD_READ_DMA       0xC8 // [28-bit LBA] Read DMA
#define ATA_CMD_READ_DMA_EXT   0x25 // [48-bit LBA] Read DMA Ext
#define ATA_CMD_WRITE_PIO      0x30 // [28-bit LBA] Write Sectors
#define ATA_CMD_WRITE_PIO_EXT  0x34 // [48-bit LBA] Write Sectors Ext
#define ATA_CMD_WRITE_DMA      0xCA // [28-bit LBA] Write DMA
#define ATA_CMD_WRITE_DMA_EXT  0x35 // [48-bit LBA] Write DMA
#define ATA_CMD_FLUSH          0xE7 // [28-bit LBA] Flush Cache
#define ATA_CMD_FLUSH_EXT      0xEA // [48-bit LBA] Flush Cache Ext
#define ATA_CMD_IDENTIFY       0xEC // Identify Device (ATA disk)
#define ATA_CMD_IDENTIFY_ATAPI 0xA1 // Identify Packet Device (ATAPI)
#define ATA_CMD_PACKET         0xA0 // Issue ATAPI SCSI packet

#define ATA_STATUS_ERR  UINT8_C(0x01) // Error - check error register for details
#define ATA_STATUS_DRQ  UINT8_C(0x08) // Data Request - device ready to transfer
#define ATA_STATUS_DF   UINT8_C(0x20) // Device Fault
#define ATA_STATUS_DRDY UINT8_C(0x40) // Device Ready
#define ATA_STATUS_BSY  UINT8_C(0x80) // Busy - no other bits valid while set

#define ATA_ERROR_ABRT UINT8_C(0x04) // Command aborted by device
#define ATA_ERROR_IDNF UINT8_C(0x10) // Sector ID not found
#define ATA_ERROR_UNC  UINT8_C(0x40) // Uncorrectable data error
#define ATA_ERROR_BBK  UINT8_C(0x80) // Bad block detected

#define ATA_CONTROL_NIEN UINT8_C(0x02) // Disable device IRQ
#define ATA_CONTROL_SRST UINT8_C(0x04) // Software reset - assert then deassert to reset channel
#define ATA_CONTROL_HOB  UINT8_C(0x80) // High Order Byte - read back high bytes of 48-bit registers

#define ATA_DEVICE_FIXED UINT8_C(0xA0) // Bits 7 and 5 must always be set when writing device register
#define ATA_DEVICE_LBA   UINT8_C(0x40) // LBA mode (vs CHS)
#define ATA_DEVICE_SLAVE UINT8_C(0x10) // Select slave device (0 -> master)

#define ATA_SIGNATURE_ATA   UINT32_C(0x00000101)
#define ATA_SIGNATURE_ATAPI UINT32_C(0xEB140101)

#define ATA_FIS_TYPE_H2D UINT8_C(0x27) // Register FIS - host to device

#define ATA_IDENTIFY_SERIAL_WORD     10
#define ATA_IDENTIFY_SERIAL_NWORDS   10
#define ATA_IDENTIFY_FIRMWARE_WORD   23
#define ATA_IDENTIFY_FIRMWARE_NWORDS 4
#define ATA_IDENTIFY_MODEL_WORD      27
#define ATA_IDENTIFY_MODEL_NWORDS    20

typedef uint16_t ata_identify_t[256];

void ata_identify_string(const uint16_t* restrict field, size_t field_nw, char* restrict string, size_t size);

static __always_inline __nodiscard bool ata_identify_is_atapi(const ata_identify_t packet)
{
    return le16toh(packet[0]) & UINT16_C(0x8000);
}

static __always_inline __nodiscard bool ata_identify_has_lba48(const ata_identify_t packet)
{
    return le16toh(packet[83]) & UINT16_C(0x0400);
}

static __always_inline __nodiscard size_t ata_identify_lba48_capacity(const ata_identify_t packet)
{
    size_t result = 0;
    result |= (size_t)le16toh(packet[100]);
    result |= (size_t)le16toh(packet[101]) << 16;
    result |= (size_t)le16toh(packet[102]) << 32;
    result |= (size_t)le16toh(packet[103]) << 48;
    return result;
}

static __always_inline __nodiscard size_t ata_identify_lba28_capacity(const ata_identify_t packet)
{
    size_t result = 0;
    result |= (size_t)le16toh(packet[60]);
    result |= (size_t)le16toh(packet[61]) << 16;
    return result;
}

static __always_inline __nodiscard size_t ata_identify_sector_count(const ata_identify_t packet)
{
    if(ata_identify_has_lba48(packet))
        return ata_identify_lba48_capacity(packet);
    return ata_identify_lba28_capacity(packet);
}

static __always_inline __nodiscard size_t ata_identify_logical_sector_size(const ata_identify_t packet)
{
    size_t result = 0;

    if(le16toh(packet[106]) & UINT16_C(0x1000)) {
        result |= (size_t)le16toh(packet[117]);
        result |= (size_t)le16toh(packet[118]) << 16;
        return result;
    }

    return 512;
}
