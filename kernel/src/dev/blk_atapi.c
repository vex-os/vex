#include <vex/blk_atapi.h>
#include <vex/blk_device.h>
#include <vex/blk_name.h>
#include <vex/errno.h>
#include <vex/string.h>

#define ATAPI_CMD_READ_10    0x28
#define ATAPI_CMD_WRITE_10   0x2A
#define ATAPI_CMD_SYNC_CACHE 0x35

static int atapi_read(const struct blk_device* restrict device, uintptr_t lba, size_t count, void* restrict buffer)
{
    const struct atapi_device* atapi = ATAPI_DEVICE_FROM_BLK(device);
    atapi_cdb_type cdb;

    memset(cdb, 0, sizeof(atapi_cdb_type));
    cdb[0] = ATAPI_CMD_READ_10;
    cdb[2] = lba >> 24;
    cdb[3] = lba >> 16;
    cdb[4] = lba >> 8;
    cdb[5] = lba & 0xFF;
    cdb[7] = count >> 8;
    cdb[8] = count & 0xFF;

    return atapi->ops->send_packet(atapi, cdb, buffer, count * device->sector_size);
}

static int atapi_write(const struct blk_device* restrict device, uintptr_t lba, size_t count, const void* restrict buffer)
{
    const struct atapi_device* atapi = ATAPI_DEVICE_FROM_BLK(device);

    if(0 == (atapi->flags & ATAPI_FLAG_WRITABLE)) {
        return -EROFS;
    }

    atapi_cdb_type cdb;
    memset(cdb, 0, sizeof(atapi_cdb_type));
    cdb[0] = ATAPI_CMD_WRITE_10;
    cdb[2] = lba >> 24;
    cdb[3] = lba >> 16;
    cdb[4] = lba >> 8;
    cdb[5] = lba & 0xFF;
    cdb[7] = count >> 8;
    cdb[8] = count & 0xFF;

    return atapi->ops->send_packet(atapi, cdb, (void*)buffer, count * device->sector_size);
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
};

int add_atapi_device(struct atapi_device* restrict device, struct blk_name* restrict name, unsigned iterator, unsigned index)
{
    device->base.ops = &atapi_blk_device_ops;

    return add_blk_device_root(&device->base, name, iterator, index);
}

void remove_atapi_device(struct atapi_device* restrict device)
{
    remove_blk_device(&device->base);
}
