#include <vex/clockevt.h>
#include <vex/delay.h>
#include <vex/device.h>
#include <vex/driver.h>
#include <vex/errno.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/intctl.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/string.h>

#define PIT_FREQUENCY UINT64_C(1193182)
#define PIT_COUNT_MAX UINT64_C(0xFFFF)
#define NS_PER_SEC    UINT64_C(1000000000)

#define PIT_PORT_BASE  0x40
#define PIT_PORT_COUNT 4
#define PIT_ISA_IRQ    0

#define PIT_REG_CHANNEL0 0
#define PIT_REG_COMMAND  3

// 8254 control word for channel 0: the latch command, or access to both bytes and a mode
#define PIT_LATCH_CHANNEL0      0x00
#define PIT_MODE_TERMINAL_COUNT 0x30 // Mode 0: the output rises once the count runs out
#define PIT_MODE_RATE           0x34 // Mode 2: the output pulses every count

#define PROBE_USEC 100

typedef struct pit_s pit_t;

struct pit_s {
    io_region_t ports;
    clock_event_t event;
    unsigned irq; // What the event was last set to raise
};

static const driver_match_t pit_matches[] = {
    DRIVER_MATCH_FIRMWARE("PNP0100"),
    DRIVER_MATCH_END(),
};

static const fwnode_resource_t pit_resources[] = {
    {
        .type = FWNODE_RESOURCE_PORT,
        .range = {
            .start = PIT_PORT_BASE,
            .length = PIT_PORT_COUNT,
        },
    },
    {
        .type = FWNODE_RESOURCE_IRQ,
        .irq = {
            .number = PIT_ISA_IRQ,
            .trigger = TRIGGER_EDGE,
            .polarity = ACTIVE_HIGH,
            .flags = FWNODE_IRQ_ISA,
        },
    },
};

static const fwnode_fixed_t pit_fixed = {
    .name = "pit",
    .id = "PNP0100",
    .resources = pit_resources,
    .num_resources = sizeof(pit_resources) / sizeof(pit_resources[0]),
};

static bool pit_bound = false;

static void load(const io_region_t* restrict ports, uint8_t mode, uint64_t count)
{
    io_write8(ports, PIT_REG_COMMAND, mode);
    io_write8(ports, PIT_REG_CHANNEL0, count & 0xFF);
    io_write8(ports, PIT_REG_CHANNEL0, (count >> 8) & 0xFF);
}

static uint16_t read_count(const io_region_t* restrict ports)
{
    io_write8(ports, PIT_REG_COMMAND, PIT_LATCH_CHANNEL0);

    uint16_t count = io_read8(ports, PIT_REG_CHANNEL0);
    count |= (uint16_t)(io_read8(ports, PIT_REG_CHANNEL0)) << 8;

    return count;
}

// Mode 0 without a count holds the output low
static void stop(const io_region_t* restrict ports)
{
    io_write8(ports, PIT_REG_COMMAND, PIT_MODE_TERMINAL_COUNT);
}

// Ports nothing decodes read back the same, and so does an
// 8254 whose clock the chipset gated off, as newer ones may
static bool counts(const io_region_t* restrict ports)
{
    load(ports, PIT_MODE_RATE, PIT_COUNT_MAX);

    uint16_t first = read_count(ports);
    delay_usec(PROBE_USEC);
    uint16_t second = read_count(ports);

    stop(ports);

    return first != second;
}

static uint64_t count_of(uint64_t nsec)
{
    uint64_t count = nsec * PIT_FREQUENCY / NS_PER_SEC;

    // Mode 2 takes no count below 2, and mode 0 counts
    // zero as the whole range rather than firing at once
    if(count < 2) {
        return 2;
    }

    if(count > PIT_COUNT_MAX) {
        return PIT_COUNT_MAX;
    }

    return count;
}

static void pit_set_oneshot(clock_event_t* restrict event, unsigned irq, uint64_t nsec)
{
    pit_t* pit = event->ctxdata;

    pit->irq = irq;
    load(&pit->ports, PIT_MODE_TERMINAL_COUNT, count_of(nsec));
}

static void pit_set_periodic(clock_event_t* restrict event, unsigned irq, uint64_t nsec)
{
    pit_t* pit = event->ctxdata;

    pit->irq = irq;
    load(&pit->ports, PIT_MODE_RATE, count_of(nsec));
}

static void pit_stop(clock_event_t* restrict event)
{
    pit_t* pit = event->ctxdata;

    stop(&pit->ports);
}

static const clock_event_ops_t pit_event_ops = {
    .set_oneshot = &pit_set_oneshot,
    .set_periodic = &pit_set_periodic,
    .stop = &pit_stop,
};

// The handler of irq may switch threads and never come back here, which would
// keep the interrupt line's lock held, so it runs as an interrupt of its own
static void pit_irq(unsigned number, const hart_context_t* restrict ctx, void* restrict ctxdata)
{
    pit_t* pit = ctxdata;
    hart_local_t* hart = this_hart();

    hart_ipi(hart, pit->irq);
}

static int pit_probe(device_t* restrict device, const driver_match_t* restrict match)
{
    return DRIVER_SCORE_DEFAULT;
}

static void init_event(pit_t* restrict pit)
{
    clock_event_t* event = &pit->event;
    event->ops = &pit_event_ops;
    event->name = "pit";
    event->rating = 100;
    event->features = CLOCK_EVENT_ONESHOT | CLOCK_EVENT_PERIODIC;
    event->min_delta_ns = (2 * NS_PER_SEC + PIT_FREQUENCY - 1) / PIT_FREQUENCY;
    event->max_delta_ns = PIT_COUNT_MAX * NS_PER_SEC / PIT_FREQUENCY;
    event->ctxdata = pit;
}

static int pit_attach(device_t* restrict device, const driver_match_t* restrict match)
{
    pit_t* pit = kmalloc(sizeof(pit_t));

    if(pit == nullptr) {
        return -ENOMEM;
    }

    memset(pit, 0, sizeof(pit_t));
    device->ctxdata = pit;

    int retval = device_map_io(device, 0, &pit->ports);

    if(retval < 0) {
        return retval;
    }

    if(!counts(&pit->ports)) {
        pr_verbose("pit: %s doesn't count", device->name);
        return -ENODEV;
    }

    retval = device_request_irq(device, 0, &pit_irq, pit);

    if(retval < 0) {
        return retval;
    }

    init_event(pit);
    retval = clock_event_add(&pit->event);

    if(retval < 0) {
        return retval;
    }

    pit_bound = true;

    return 0;
}

static int pit_offline(device_t* restrict device)
{
    pit_t* pit = device->ctxdata;

    if(clock_event_remove(&pit->event) == -EBUSY) {
        pr_warning("pit: %s is the only timer of a hart", device->name);
        return -EBUSY;
    }

    return 0;
}

static void pit_online(device_t* restrict device)
{
    pit_t* pit = device->ctxdata;

    clock_event_add(&pit->event);
}

// A hart without another timer keeps this one, as a hart without any never schedules again
static void pit_detach(device_t* restrict device)
{
    pit_t* pit = device->ctxdata;

    if(device_is_gone(device) && clock_event_remove(&pit->event) == -EBUSY) {
        pr_warning("pit: %s is gone, but a hart has no other timer", device->name);
    }

    pit_bound = false;
}

static void pit_release(device_t* restrict device)
{
    kfree(device->ctxdata);
}

static const driver_ops_t pit_driver_ops = {
    .probe = &pit_probe,
    .attach = &pit_attach,
    .offline = &pit_offline,
    .online = &pit_online,
    .detach = &pit_detach,
    .release = &pit_release,
};

define_driver(pit, &pit_driver_ops, pit_matches);

// After the tree is first bound, so the firmware's own node gets the PIT if it has one;
// PCs have had an 8254 at these ports since the first, but firmware doesn't always say
static void arch_init_pit_node(void)
{
    io_region_t ports = {};
    ports.space = IO_SPACE_PORT;
    ports.base = PIT_PORT_BASE;
    ports.length = PIT_PORT_COUNT;

    if(pit_bound || !counts(&ports)) {
        return;
    }

    device_t* root = device_get_root();

    if(root == nullptr) {
        pr_warning("pit: out of memory");
        return;
    }

    fwnode_t fwnode;
    fwnode_init_fixed(&fwnode, &pit_fixed);

    char name[DEVICE_NAME_MAX];
    device_t* device;

    if(fwnode_get_name(&fwnode, name, sizeof(name)) < 0 || device_add(root, &fwnode, name, &device) < 0) {
        pr_warning("pit: out of memory");
        device_decref(root);
        return;
    }

    device_rescan(device);
    device_decref(device);
    device_decref(root);
}

initcall_bsp_dev(arch_init_pit_node, INIT_LAST);
