#include <vex/blk_atapi.h>
#include <vex/blk_device.h>
#include <vex/errno.h>
#include <vex/string.h>

#define ATAPI_CMD_READ_10    0x28
#define ATAPI_CMD_WRITE_10   0x2A
#define ATAPI_CMD_SYNC_CACHE 0x35

static void setup_rw_cdb(atapi_cdb_t cdb, uint8_t command, uintptr_t lba, size_t count)
{
    memset(cdb, 0, sizeof(atapi_cdb_t));
    cdb[0] = command;
    cdb[2] = lba >> 24;
    cdb[3] = lba >> 16;
    cdb[4] = lba >> 8;
    cdb[5] = lba & 0xFF;
    cdb[7] = count >> 8;
    cdb[8] = count & 0xFF;
}

static int check_request(const atapi_device_t* restrict atapi, const blk_request_t* restrict req, size_t count)
{
    if(count > UINT16_MAX) {
        return -EINVAL;
    }

    if(req->op == BLK_REQ_WRITE && 0 == (atapi->flags & ATAPI_FLAG_WRITABLE)) {
        return -EROFS;
    }

    return 0;
}

static void atapi_submit(const blk_device_t* restrict device, blk_request_t* restrict req)
{
    const atapi_device_t* atapi = ATAPI_DEVICE_FROM_BLK(device);
    size_t count = req->length / device->sector_size;
    int retval = check_request(atapi, req, count);
    atapi_cdb_t cdb;

    if(retval < 0) {
        req->status = retval;
        req->done(req);
        return;
    }

    switch(req->op) {
        case BLK_REQ_READ:
            setup_rw_cdb(cdb, ATAPI_CMD_READ_10, req->lba, count);
            break;

        case BLK_REQ_WRITE:
            setup_rw_cdb(cdb, ATAPI_CMD_WRITE_10, req->lba, count);
            break;

        default:
            memset(cdb, 0, sizeof(atapi_cdb_t));
            cdb[0] = ATAPI_CMD_SYNC_CACHE;
            break;
    }

    atapi->ops->submit_packet(atapi, cdb, req);
}

static void atapi_release(const blk_device_t* restrict device)
{
    const atapi_device_t* atapi = ATAPI_DEVICE_FROM_BLK(device);

    if(atapi->ops->release) {
        atapi->ops->release(atapi);
    }
}

static const blk_device_ops_t atapi_blk_device_ops = {
    .submit = &atapi_submit,
    .release = &atapi_release,
};

int atapi_device_add(atapi_device_t* restrict device, dev_unit_t* restrict unit, unsigned index)
{
    device->base.ops = &atapi_blk_device_ops;

    return blk_device_add_root(&device->base, unit, index);
}

void atapi_device_remove(atapi_device_t* restrict device)
{
    blk_device_remove(&device->base);
}
