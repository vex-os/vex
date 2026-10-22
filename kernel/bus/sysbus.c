#include <vex/device.h>
#include <vex/errno.h>
#include <vex/fwnode.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mutex.h>
#include <vex/string.h>
#include <vex/sysbus.h>

#define SYSBUS_ID_MAX 16

static mutex_t sysbus_mutex = MUTEX_INIT;
static sysbus_device_t* device_head = nullptr;
static sysbus_device_t** device_tail = &device_head;
static sysbus_driver_t* driver_head = nullptr;

static void device_bind(sysbus_device_t* restrict device, sysbus_driver_t* restrict driver)
{
    if(device->priv.driver || !fwnode_matches(&device->priv.device->fwnode, driver->ids)) {
        return;
    }

    if(0 == driver->ops->probe(device)) {
        device->priv.driver = driver;
    }
}

void sysbus_device_add(device_t* restrict device)
{
    char id[SYSBUS_ID_MAX];

    if(fwnode_get_id(&device->fwnode, id, sizeof(id)) < 0) {
        return;
    }

    sysbus_device_t* sysbus_device = kmalloc(sizeof(sysbus_device_t));
    char* id_copy = kstrdup(id);

    if(sysbus_device == nullptr || id_copy == nullptr) {
        pr_warning("sysbus: out of memory");
        kfree(id_copy);
        kfree(sysbus_device);
        return;
    }

    device->bus_ctxdata = sysbus_device;

    sysbus_device->name = id_copy;
    sysbus_device->priv.next = nullptr;
    sysbus_device->priv.driver = nullptr;
    sysbus_device->priv.device = device;
    sysbus_device->ctxdata = nullptr;

    mutex_acquire(&sysbus_mutex);

    *device_tail = sysbus_device;
    device_tail = &sysbus_device->priv.next;

    for(sysbus_driver_t* it = driver_head; it; it = it->priv.next) {
        device_bind(sysbus_device, it);
    }

    mutex_release(&sysbus_mutex);
}

int sysbus_driver_add(sysbus_driver_t* restrict driver)
{
    mutex_acquire(&sysbus_mutex);

    for(sysbus_driver_t* it = driver_head; it; it = it->priv.next) {
        if(it == driver) {
            mutex_release(&sysbus_mutex);
            return -EEXIST;
        }
    }

    driver->priv.next = driver_head;
    driver_head = driver;

    for(sysbus_device_t* it = device_head; it; it = it->priv.next) {
        device_bind(it, driver);
    }

    mutex_release(&sysbus_mutex);

    return 0;
}

int sysbus_driver_remove(sysbus_driver_t* restrict driver)
{
    mutex_acquire(&sysbus_mutex);

    sysbus_driver_t** prev = &driver_head;

    while(*prev && *prev != driver) {
        prev = &(*prev)->priv.next;
    }

    if(*prev == nullptr) {
        mutex_release(&sysbus_mutex);
        return -ENODEV;
    }

    *prev = driver->priv.next;

    for(sysbus_device_t* it = device_head; it; it = it->priv.next) {
        if(it->priv.driver == driver) {
            if(driver->ops->remove) {
                driver->ops->remove(it);
            }

            it->priv.driver = nullptr;
        }
    }

    mutex_release(&sysbus_mutex);

    return 0;
}

void sysbus_get_node(const sysbus_device_t* restrict device, fwnode_t* restrict node)
{
    *node = device->priv.device->fwnode;
}

int sysbus_map_io(sysbus_device_t* restrict device, unsigned index, io_region_t* restrict region)
{
    return device_map_io(device->priv.device, index, region);
}

void sysbus_unmap_io(io_region_t* restrict region)
{
    device_unmap_io(region);
}

int sysbus_request_irq(sysbus_device_t* restrict device, unsigned index, irq_handler_t handler, void* restrict ctxdata)
{
    return device_request_irq(device->priv.device, index, handler, ctxdata);
}

void sysbus_release_irq(sysbus_device_t* restrict device, unsigned index)
{
    device_release_irq(device->priv.device, index);
}
