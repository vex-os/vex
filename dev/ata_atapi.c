#include <core/errno.h>
#include <dev/ata_atapi.h>
#include <dev/blk_device.h>
#include <dev/blk_name.h>
#include <lib/string.h>

#define ATAPI_CMD_READ_10    0x28
#define ATAPI_CMD_WRITE_10   0x2A
#define ATAPI_CMD_SYNC_CACHE 0x35

static int atapi_read(const struct blk_device* restrict device, uintptr_t lba, size_t count, void* restrict buffer)
{
    const struct atapi_device* atapi = ATAPI_DEVICE_FROM_BLK(device);
    atapi_cdb_type cdb;

    memset(cdb, 0, sizeof(atapi_cdb_type));
    cdb[0] = ATAPI_CMD_READ_10;
    cdb[2] = (uint8_t)(lba >> 24);
    cdb[3] = (uint8_t)(lba >> 16);
    cdb[4] = (uint8_t)(lba >> 8);
    cdb[5] = (uint8_t)(lba >> 0);
    cdb[7] = (uint8_t)(count >> 8);
    cdb[8] = (uint8_t)(count >> 0);

    return atapi->ops->send_packet(atapi, cdb, buffer, count * device->sector_size);
}

static int atapi_write(const struct blk_device* restrict device, uintptr_t lba, size_t count, const void* restrict buffer)
{
    const struct atapi_device* atapi = ATAPI_DEVICE_FROM_BLK(device);

    atapi_cdb_type cdb;

    if(0 == (atapi->flags & ATAPI_FLAG_WRITABLE)) {
        return -EROFS;
    }

    memset(cdb, 0, sizeof(atapi_cdb_type));
    cdb[0] = ATAPI_CMD_WRITE_10;
    cdb[2] = (uint8_t)(lba >> 24);
    cdb[3] = (uint8_t)(lba >> 16);
    cdb[4] = (uint8_t)(lba >> 8);
    cdb[5] = (uint8_t)(lba >> 0);
    cdb[7] = (uint8_t)(count >> 8);
    cdb[8] = (uint8_t)(count >> 0);

    return atapi->ops->send_packet(atapi, cdb, (void*)(buffer), count * device->sector_size);
}

static int atapi_flush(const struct blk_device* restrict device)
{
    const struct atapi_device* atapi = ATAPI_DEVICE_FROM_BLK(device);

    atapi_cdb_type cdb;

    memset(cdb, 0, sizeof(atapi_cdb_type));
    cdb[0] = ATAPI_CMD_SYNC_CACHE;

    return atapi->ops->send_packet(atapi, cdb, nullptr, 0);
}

static const struct blk_device_ops atapi_blk_device_ops = {
    .read = &atapi_read,
    .write = &atapi_write,
    .flush = &atapi_flush,
    .destroy = nullptr,
};

int atapi_register(struct atapi_device* restrict device, struct blk_name* restrict name, unsigned iterator, unsigned index)
{
    device->base.ops = &atapi_blk_device_ops;

    return blk_device_register_root(&device->base, name, iterator, index);
}

void atapi_unregister(struct atapi_device* restrict device)
{
    blk_device_unregister(&device->base);
}
