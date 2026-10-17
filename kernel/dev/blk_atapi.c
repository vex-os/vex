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

static void atapi_submit(const struct blk_device* restrict device, struct blk_request* restrict req)
{
    const struct atapi_device* atapi = ATAPI_DEVICE_FROM_BLK(device);
    size_t count = req->length / device->sector_size;
    atapi_cdb_type cdb;

    if(count > UINT16_MAX) {
        req->status = -EINVAL;
        req->done(req);
        return;
    }

    switch(req->op) {
        case BLK_REQ_READ:
            setup_rw_cdb(cdb, ATAPI_CMD_READ_10, req->lba, count);
            break;

        case BLK_REQ_WRITE:
            if(0 == (atapi->flags & ATAPI_FLAG_WRITABLE)) {
                req->status = -EROFS;
                req->done(req);
                return;
            }

            setup_rw_cdb(cdb, ATAPI_CMD_WRITE_10, req->lba, count);
            break;

        default:
            memset(cdb, 0, sizeof(atapi_cdb_type));
            cdb[0] = ATAPI_CMD_SYNC_CACHE;
            break;
    }

    atapi->ops->submit_packet(atapi, cdb, req);
}

static void atapi_release(const struct blk_device* restrict device)
{
    const struct atapi_device* atapi = ATAPI_DEVICE_FROM_BLK(device);

    if(atapi->ops->release) {
        atapi->ops->release(atapi);
    }
}

static const struct blk_device_ops atapi_blk_device_ops = {
    .submit = &atapi_submit,
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
