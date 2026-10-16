#include <vex/blk_atapi.h>
#include <vex/blk_device.h>
#include <vex/blk_name.h>
#include <vex/errno.h>
#include <vex/string.h>

#define ATAPI_CMD_READ_10    0x28
#define ATAPI_CMD_WRITE_10   0x2A
#define ATAPI_CMD_SYNC_CACHE 0x35

static void setup_rw_cdb(atapi_cdb_type cdb, uint8_t command, uintptr_t lba, size_t count)
{
    memset(cdb, 0, sizeof(atapi_cdb_type));
    cdb[0] = command;
    cdb[2] = lba >> 24;
    cdb[3] = lba >> 16;
    cdb[4] = lba >> 8;
    cdb[5] = lba & 0xFF;
    cdb[7] = count >> 8;
    cdb[8] = count & 0xFF;
}

static size_t max_packet_sectors(const struct atapi_device* restrict atapi)
{
    size_t result = UINT16_MAX; // CDB transfer length is 16-bit

    if(atapi->max_transfer) {
        size_t limit = atapi->max_transfer / atapi->base.sector_size;

        if(limit < result) {
            result = limit;
        }
    }

    return result;
}

static int atapi_transfer(const struct atapi_device* restrict atapi, uint8_t command, uintptr_t lba, size_t count, uint8_t* restrict buffer)
{
    size_t sector_size = atapi->base.sector_size;
    size_t max_sectors = max_packet_sectors(atapi);

    if(sector_size == 0 || max_sectors == 0) {
        return -EIO;
    }

    while(count > 0) {
        size_t chunk = count < max_sectors ? count : max_sectors;
        atapi_cdb_type cdb;

        setup_rw_cdb(cdb, command, lba, chunk);

        int retval = atapi->ops->send_packet(atapi, cdb, buffer, chunk * sector_size);

        if(retval < 0) {
            return retval;
        }

        buffer += chunk * sector_size;
        lba += chunk;
        count -= chunk;
    }

    return 0;
}

static int atapi_read(const struct blk_device* restrict device, uintptr_t lba, size_t count, void* restrict buffer)
{
    return atapi_transfer(ATAPI_DEVICE_FROM_BLK(device), ATAPI_CMD_READ_10, lba, count, buffer);
}

static int atapi_write(const struct blk_device* restrict device, uintptr_t lba, size_t count, const void* restrict buffer)
{
    const struct atapi_device* atapi = ATAPI_DEVICE_FROM_BLK(device);

    if(0 == (atapi->flags & ATAPI_FLAG_WRITABLE)) {
        return -EROFS;
    }

    return atapi_transfer(atapi, ATAPI_CMD_WRITE_10, lba, count, (uint8_t*)(buffer)); // Only ever read for a write
}

static int atapi_flush(const struct blk_device* restrict device)
{
    const struct atapi_device* atapi = ATAPI_DEVICE_FROM_BLK(device);

    atapi_cdb_type cdb;

    memset(cdb, 0, sizeof(atapi_cdb_type));
    cdb[0] = ATAPI_CMD_SYNC_CACHE;

    return atapi->ops->send_packet(atapi, cdb, nullptr, 0);
}

static void atapi_release(const struct blk_device* restrict device)
{
    const struct atapi_device* atapi = ATAPI_DEVICE_FROM_BLK(device);

    if(atapi->ops->release) {
        atapi->ops->release(atapi);
    }
}

static const struct blk_device_ops atapi_blk_device_ops = {
    .read = &atapi_read,
    .write = &atapi_write,
    .flush = &atapi_flush,
    .release = &atapi_release,
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
