#include <vex/block_atapi.h>
#include <vex/block_interface.h>
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

static int check_request(const atapi_interface_t* restrict atapi, const block_request_t* restrict req, size_t count)
{
    if(count > UINT16_MAX) {
        return -EINVAL;
    }

    if(req->op == BLOCK_REQ_WRITE && 0 == (atapi->flags & ATAPI_FLAG_WRITABLE)) {
        return -EROFS;
    }

    return 0;
}

static void atapi_submit(const block_interface_t* restrict interface, block_request_t* restrict req)
{
    const atapi_interface_t* atapi = ATAPI_INTERFACE_FROM_BLOCK(interface);
    size_t count = req->length / interface->sector_size;
    int retval = check_request(atapi, req, count);
    atapi_cdb_t cdb;

    if(retval < 0) {
        req->status = retval;
        req->done(req);
        return;
    }

    switch(req->op) {
        case BLOCK_REQ_READ:
            setup_rw_cdb(cdb, ATAPI_CMD_READ_10, req->lba, count);
            break;

        case BLOCK_REQ_WRITE:
            setup_rw_cdb(cdb, ATAPI_CMD_WRITE_10, req->lba, count);
            break;

        default:
            memset(cdb, 0, sizeof(atapi_cdb_t));
            cdb[0] = ATAPI_CMD_SYNC_CACHE;
            break;
    }

    atapi->ops->submit_packet(atapi, cdb, req);
}

static void atapi_release(const block_interface_t* restrict interface)
{
    const atapi_interface_t* atapi = ATAPI_INTERFACE_FROM_BLOCK(interface);

    if(atapi->ops->release) {
        atapi->ops->release(atapi);
    }
}

static const block_interface_ops_t atapi_block_interface_ops = {
    .submit = &atapi_submit,
    .release = &atapi_release,
};

int atapi_interface_add(atapi_interface_t* restrict interface, dev_unit_t* restrict unit, unsigned index)
{
    interface->base.ops = &atapi_block_interface_ops;

    return block_interface_add_root(&interface->base, unit, index);
}

void atapi_interface_remove(atapi_interface_t* restrict interface)
{
    block_interface_remove(&interface->base);
}
