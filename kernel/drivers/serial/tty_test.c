#include <vex/chr_device.h>
#include <vex/clocksrc.h>
#include <vex/dev_name.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/string.h>
#include <vex/tty_device.h>

// Long enough for tty_worker to echo what was just written to the line
#define LINE_WAIT_NSEC UINT64_C(200000000)

static tty_device_t test_tty;
static chr_device_t test_line;
static event_t line_ready;

static void test_tty_start_tx(tty_device_t* restrict device)
{
    event_trigger(&line_ready, true);
}

static const tty_device_ops_t test_tty_ops = {
    .start_tx = &test_tty_start_tx,
};

static ssize_t test_line_read(const chr_device_t* restrict device, off_t offset, void* restrict buffer, size_t size, unsigned flags)
{
    uint64_t deadline = clock_source_now_ns() + LINE_WAIT_NSEC;

    for(;;) {
        size_t taken = tty_transmit_take(&test_tty, buffer, size);
        uint64_t now = clock_source_now_ns();

        if(taken || now >= deadline) {
            return taken;
        }

        // A wakeup left over from an earlier start_tx only makes this look again
        event_t* ready = &line_ready;
        ssize_t result = event_await_timeout(&ready, 1, deadline - now);

        if(result < 0 && result != -ETIMEDOUT) {
            return result;
        }
    }
}

static ssize_t test_line_write(const chr_device_t* restrict device, off_t offset, const void* restrict buffer, size_t size, unsigned flags)
{
    tty_receive(&test_tty, buffer, size);
    return size;
}

static const chr_device_ops_t test_line_ops = {
    .read = &test_line_read,
    .write = &test_line_write,
};

static void init_tty_test(void)
{
    dev_unit_t* tty_unit;
    dev_unit_t* line_unit;

    event_init(&line_ready);

    if(dev_unit_alloc(&dev_prefix_tty, &tty_unit) < 0 || dev_unit_alloc(&dev_prefix_pty, &line_unit) < 0) {
        pr_warning("tty_test: no unit numbers");
        return;
    }

    test_tty.ops = &test_tty_ops;
    test_line.ops = &test_line_ops;

    int retval = tty_device_add(&test_tty, tty_unit, DEV_NAME_NONE);

    if(retval == 0) {
        retval = chr_device_add(&test_line, line_unit, DEV_NAME_NONE);
    }

    if(retval < 0) {
        pr_warning("tty_test: %s", strerror(retval));
        return;
    }

    pr_inform("tty_test: %s, its line %s", test_tty.chr.name, test_line.name);
}

initcall_bsp_dev(init_tty_test, INIT_MIDDLE);
