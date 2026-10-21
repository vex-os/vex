#include "vex/serial.h"
#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/capability.h>
#include <vex/clocksrc.h>
#include <vex/cmdline.h>
#include <vex/cred.h>
#include <vex/ctype.h>
#include <vex/dev_name.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/kprintf.h>
#include <vex/serial_device.h>
#include <vex/stdlib.h>
#include <vex/string.h>
#include <vex/strings.h>
#include <vex/thread.h>
#include <vex/uaccess.h>

#define NSEC_PER_MSEC UINT64_C(1000000)

#define BREAK_DEFAULT_MSEC 250
#define BREAK_MAX_MSEC     10000
#define EXCHANGE_MAX_MSEC  60000
#define REMOVE_RETRY_MSEC  10

typedef struct exchange_s exchange_t;

// Under the port's dispatch lock
struct exchange_s {
    const uint8_t* send;
    uint8_t* receive;
    uint32_t send_size;
    uint32_t sent;
    uint32_t receive_size;
    uint32_t received;
    int32_t terminator;
    bool done;
    event_t finished;
};

static spinlock_t consumers_lock = SPINLOCK_INIT;
static serial_consumer_t* consumers_head = nullptr;

static serial_device_t* serial_of(const chr_device_t* restrict chr)
{
    return chr->ctxdata;
}

static bool is_removed(const serial_device_t* restrict device)
{
    return atomic_load(&device->priv.removed, ATOMIC_ACQUIRE);
}

static bool is_terminated(const char* restrict buffer, size_t size)
{
    return (bool)(memchr(buffer, 0, size));
}

static void sleep_msec(uint32_t msec)
{
    event_t never;
    event_t* events = &never;

    event_init(&never);
    event_await_timeout(&events, 1, msec * NSEC_PER_MSEC);
    event_deinit(&never);
}

static void set_dispatch(serial_device_t* restrict device, const serial_consumer_t* restrict consumer)
{
    bool intr_state = spinlock_acquire_intr(&device->priv.dispatch_lock);
    device->priv.consumer = consumer;
    spinlock_release_intr(&device->priv.dispatch_lock, intr_state);
}

static bool line_is_valid(const serial_line_t* restrict line)
{
    bool result = true;
    result = result && line->speed;
    result = result && line->data_bits >= 5 && line->data_bits <= 8;
    result = result && line->stop_bits >= 1 && line->stop_bits <= 2;
    result = result && line->parity <= SERIAL_PARITY_SPACE;
    result = result && 0 == (line->flow & ~SERIAL_FLOW_RTSCTS);
    return result;
}

static int startup_locked(serial_device_t* restrict device)
{
    if(device->priv.started) {
        return 0;
    }

    if(device->ops->startup) {
        int retval = device->ops->startup(device);

        if(retval < 0) {
            return retval;
        }
    }

    device->priv.started = true;

    return 0;
}

static void shutdown_locked(serial_device_t* restrict device)
{
    if(!device->priv.started) {
        return;
    }

    if(device->ops->shutdown) {
        device->ops->shutdown(device);
    }

    device->priv.started = false;
}

static int set_line_locked(serial_device_t* restrict device, const serial_line_t* restrict line)
{
    if(!line_is_valid(line) || line->speed > device->max_speed) {
        return -EINVAL;
    }

    int retval = device->ops->set_line(device, line);

    if(retval == 0) {
        device->line = *line;
    }

    return retval;
}

static void set_modem_locked(serial_device_t* restrict device, uint32_t raise, uint32_t lower)
{
    uint32_t outputs = device->modem_lines & (SERIAL_MODEM_DTR | SERIAL_MODEM_RTS);

    device->priv.modem = ((device->priv.modem & ~lower) | raise) & outputs;

    if(device->ops->set_modem) {
        device->ops->set_modem(device, device->priv.modem);
    }
}

static uint32_t get_modem_locked(serial_device_t* restrict device)
{
    if(device->ops->get_modem) {
        return device->ops->get_modem(device);
    }

    return device->priv.modem;
}

static int send_break_locked(serial_device_t* restrict device, uint32_t msec)
{
    if(device->ops->set_break == nullptr) {
        return -EINVAL;
    }

    if(msec == 0) {
        msec = BREAK_DEFAULT_MSEC;
    }

    device->ops->set_break(device, true);
    sleep_msec(msec);
    device->ops->set_break(device, false);

    return 0;
}

static const serial_consumer_t* find_consumer(const char* restrict name)
{
    spinlock_acquire(&consumers_lock);

    serial_consumer_t* consumer = consumers_head;

    while(consumer && strcmp(consumer->name, name)) {
        consumer = consumer->next;
    }

    spinlock_release(&consumers_lock);

    return consumer;
}

static int bind_locked(serial_device_t* restrict device, serial_binding_t* restrict binding)
{
    if(is_removed(device)) {
        return -ENODEV;
    }

    if(device->priv.consumer || device->priv.unbinding) {
        return -EBUSY;
    }

    const serial_consumer_t* consumer = find_consumer(binding->consumer);

    if(consumer == nullptr) {
        return -ENOENT;
    }

    int retval = consumer->bind(device, binding->options, &binding->index, binding->object, sizeof(binding->object));

    if(retval < 0) {
        return retval;
    }

    set_dispatch(device, consumer);
    device->priv.binding = *binding;

    return 0;
}

// The consumer's unbind runs without the lock
// as the last calls of what it takes away may need it
static int unbind(serial_device_t* restrict device, bool force)
{
    const serial_consumer_t* consumer = nullptr;
    int retval = 0;

    mutex_acquire(&device->priv.lock);

    if(device->priv.unbinding) {
        retval = -EBUSY;
    }
    else if(device->priv.binding.consumer[0] == 0) {
        retval = -ENXIO;
    }
    else {
        consumer = device->priv.consumer;
        device->priv.unbinding = true;
    }

    mutex_release(&device->priv.lock);

    if(consumer == nullptr) {
        return retval;
    }

    retval = consumer->unbind(device, force);

    mutex_acquire(&device->priv.lock);

    device->priv.unbinding = false;

    if(retval == 0) {
        set_dispatch(device, nullptr);
        memset(&device->priv.binding, 0, sizeof(device->priv.binding));
        device->consumer_data = nullptr;
        shutdown_locked(device);
    }

    mutex_release(&device->priv.lock);

    return retval;
}

static void bind_from_cmdline_item(serial_device_t* restrict device, const char* restrict item, size_t length)
{
    const char* colon = memchr(item, ':', length);
    size_t port_length = colon - item;

    if(colon == nullptr || port_length != strlen(device->chr.name) || strncmp(item, device->chr.name, port_length)) {
        return;
    }

    const char* name = colon + 1;
    size_t name_length = length - port_length - 1;
    size_t letters = 0;

    while(letters < name_length && !isdigit(name[letters])) {
        letters += 1;
    }

    serial_binding_t binding = {};
    binding.index = SERIAL_ANY_INDEX;

    if(letters == 0 || letters >= sizeof(binding.consumer)) {
        pr_warning("serial: %s: bad serial.bind= consumer", device->chr.name);
        return;
    }

    memcpy(binding.consumer, name, letters);

    if(letters < name_length) {
        binding.index = strtol(&name[letters], nullptr, 10);
    }

    mutex_acquire(&device->priv.lock);
    int retval = bind_locked(device, &binding);
    mutex_release(&device->priv.lock);

    if(retval < 0) {
        pr_warning("serial: %s: binding %s: %s", device->chr.name, binding.consumer, strerror(retval));
        return;
    }

    pr_inform("serial: %s bound to %s", device->chr.name, binding.object);
}

// serial.bind=serial0:tty0,serial1:tty binds by the port's
// name; an index after the consumer's name is the one to give
static void bind_from_cmdline(serial_device_t* restrict device)
{
    const char* list = cmdline_argument(&sys_cmdline, "serial.bind");

    while(list && *list) {
        const char* comma = strchr(list, ',');

        if(comma == nullptr) {
            bind_from_cmdline_item(device, list, strlen(list));

            list = nullptr;
        }
        else {
            bind_from_cmdline_item(device, list, (size_t)(comma - list));

            list = comma + 1;
        }
    }
}

static void take_answer(exchange_t* restrict exchange, uint8_t byte)
{
    exchange->receive[exchange->received] = byte;
    exchange->received += 1;

    if(exchange->received == exchange->receive_size || (int32_t)(byte) == exchange->terminator) {
        exchange->done = true;
        event_trigger(&exchange->finished, false);
    }
}

static void exchange_receive(serial_device_t* restrict device, const uint8_t* restrict bytes, size_t count)
{
    exchange_t* exchange = device->consumer_data;

    for(size_t i = 0; i < count && !exchange->done; ++i) {
        take_answer(exchange, bytes[i]);
    }
}

static void exchange_receive_error(serial_device_t* restrict device, uint8_t byte, unsigned error)
{
    exchange_t* exchange = device->consumer_data;

    if(error != SERIAL_ERROR_BREAK && !exchange->done) {
        take_answer(exchange, byte);
    }
}

// An exchange that reads nothing back is done once its bytes are taken
static size_t exchange_transmit_take(serial_device_t* restrict device, void* restrict buffer, size_t size)
{
    exchange_t* exchange = device->consumer_data;
    size_t count = exchange->send_size - exchange->sent;

    if(count > size) {
        count = size;
    }

    memcpy(buffer, &exchange->send[exchange->sent], count);
    exchange->sent += count;

    if(exchange->receive_size == 0 && exchange->sent == exchange->send_size && !exchange->done) {
        exchange->done = true;
        event_trigger(&exchange->finished, false);
    }

    return count;
}

// Not registered, so nothing binds it; an exchange
// puts it in place for as long as it lasts
static const serial_consumer_t exchange_consumer = {
    .name = "exchange",
    .receive = &exchange_receive,
    .receive_error = &exchange_receive_error,
    .transmit_take = &exchange_transmit_take,
};

static int wait_exchanged(serial_device_t* restrict device, exchange_t* restrict exchange, uint32_t timeout_msec)
{
    uint64_t deadline = clock_source_now_ns() + timeout_msec * NSEC_PER_MSEC;
    int retval = 0;

    bool intr_state = spinlock_acquire_intr(&device->priv.dispatch_lock);

    while(!exchange->done && !is_removed(device)) {
        uint64_t now = clock_source_now_ns();

        if(now >= deadline) {
            break;
        }

        event_t* finished = &exchange->finished;
        ssize_t result = event_await_unlock(&finished, 1, &device->priv.dispatch_lock, intr_state, deadline - now);

        intr_state = spinlock_acquire_intr(&device->priv.dispatch_lock);

        if(result == -ETIMEDOUT) {
            break;
        }

        // Running the system call again
        // would send the bytes twice
        if(result < 0) {
            if(result == -ERESTART) {
                retval = -EINTR;
                break;
            }
            else {
                retval = (int)(result);
                break;
            }
        }
    }

    device->priv.consumer = nullptr;

    spinlock_release_intr(&device->priv.dispatch_lock, intr_state);

    return retval;
}

static int run_exchange_locked(serial_device_t* restrict device, exchange_t* restrict exchange, uint32_t timeout_msec)
{
    if(is_removed(device)) {
        return -ENODEV;
    }

    if(device->priv.consumer || device->priv.unbinding) {
        return -EBUSY;
    }

    bool was_started = device->priv.started;
    int retval = startup_locked(device);

    if(retval < 0) {
        return retval;
    }

    device->consumer_data = exchange;
    set_dispatch(device, &exchange_consumer);

    if(exchange->send_size) {
        device->ops->start_tx(device);
    }

    retval = wait_exchanged(device, exchange, timeout_msec);

    device->consumer_data = nullptr;

    if(!was_started) {
        shutdown_locked(device);
    }

    return retval;
}

static int exchange_bytes(serial_device_t* restrict device, uintptr_t argument)
{
    serial_exchange_t request;
    uint8_t send[SERIAL_EXCHANGE_MAX];
    uint8_t receive[SERIAL_EXCHANGE_MAX];

    if(user_copyin(&request, argument, sizeof(request)) < 0) {
        return -EFAULT;
    }

    if(request.send_size > SERIAL_EXCHANGE_MAX || request.receive_size > SERIAL_EXCHANGE_MAX) {
        return -EINVAL;
    }

    if(request.timeout_msec > EXCHANGE_MAX_MSEC) {
        return -EINVAL;
    }

    if(request.terminator < SERIAL_NO_TERMINATOR || request.terminator > UINT8_MAX) {
        return -EINVAL;
    }

    if(user_copyin(send, request.send, request.send_size) < 0) {
        return -EFAULT;
    }

    exchange_t exchange = {};
    exchange.send = send;
    exchange.receive = receive;
    exchange.send_size = request.send_size;
    exchange.receive_size = request.receive_size;
    exchange.terminator = request.terminator;
    event_init(&exchange.finished);

    mutex_acquire(&device->priv.lock);
    int retval = run_exchange_locked(device, &exchange, request.timeout_msec);
    mutex_release(&device->priv.lock);

    event_deinit(&exchange.finished);

    if(retval < 0) {
        return retval;
    }

    request.receive_size = exchange.received;

    if(user_copyout(request.receive, receive, exchange.received) < 0) {
        return -EFAULT;
    }

    if(user_copyout(argument, &request, sizeof(request)) < 0) {
        return -EFAULT;
    }

    return 0;
}

static int get_info(serial_device_t* restrict device, uintptr_t argument)
{
    serial_info_t info = {};

    // The structure starts zeroed, so a string as long as its field stays terminated
    kstrncpy(info.driver, device->driver, sizeof(info.driver) - 1);

    if(device->firmware_id) {
        kstrncpy(info.firmware_id, device->firmware_id, sizeof(info.firmware_id) - 1);
    }

    info.max_speed = device->max_speed;
    info.modem_lines = device->modem_lines;

    if(user_copyout(argument, &info, sizeof(info)) < 0) {
        return -EFAULT;
    }

    return 0;
}

static int get_line(serial_device_t* restrict device, uintptr_t argument)
{
    mutex_acquire(&device->priv.lock);
    serial_line_t line = device->line;
    mutex_release(&device->priv.lock);

    if(user_copyout(argument, &line, sizeof(line)) < 0) {
        return -EFAULT;
    }

    return 0;
}

static int set_line(serial_device_t* restrict device, uintptr_t argument)
{
    serial_line_t line;

    if(user_copyin(&line, argument, sizeof(line)) < 0) {
        return -EFAULT;
    }

    return serial_device_set_line(device, &line);
}

static int get_modem(serial_device_t* restrict device, uintptr_t argument)
{
    mutex_acquire(&device->priv.lock);
    uint32_t lines = get_modem_locked(device);
    mutex_release(&device->priv.lock);

    if(user_copyout(argument, &lines, sizeof(lines)) < 0) {
        return -EFAULT;
    }

    return 0;
}

static int set_modem(serial_device_t* restrict device, uintptr_t argument)
{
    serial_modem_t modem;

    if(user_copyin(&modem, argument, sizeof(modem)) < 0) {
        return -EFAULT;
    }

    serial_device_set_modem(device, modem.raise, modem.lower);

    return 0;
}

static int send_break(serial_device_t* restrict device, uintptr_t argument)
{
    int32_t msec;

    if(user_copyin(&msec, argument, sizeof(msec)) < 0) {
        return -EFAULT;
    }

    if(msec < 0 || msec > BREAK_MAX_MSEC) {
        return -EINVAL;
    }

    return serial_device_send_break(device, msec);
}

static int bind(serial_device_t* restrict device, uintptr_t argument)
{
    serial_binding_t binding;

    if(user_copyin(&binding, argument, sizeof(binding)) < 0) {
        return -EFAULT;
    }

    if(!is_terminated(binding.consumer, sizeof(binding.consumer))) {
        return -EINVAL;
    }

    if(!is_terminated(binding.options, sizeof(binding.options))) {
        return -EINVAL;
    }

    if(binding.index < SERIAL_ANY_INDEX) {
        return -EINVAL;
    }

    memset(binding.object, 0, sizeof(binding.object));

    mutex_acquire(&device->priv.lock);
    int retval = bind_locked(device, &binding);
    mutex_release(&device->priv.lock);

    if(retval == 0) {
        if(user_copyout(argument, &binding, sizeof(binding)) < 0) {
            return -EFAULT;
        }
    }

    return retval;
}

static int get_binding(serial_device_t* restrict device, uintptr_t argument)
{
    mutex_acquire(&device->priv.lock);
    serial_binding_t binding = device->priv.binding;
    mutex_release(&device->priv.lock);

    if(user_copyout(argument, &binding, sizeof(binding)) < 0) {
        return -EFAULT;
    }

    return 0;
}

static bool changes_port(uint32_t request)
{
    switch(request) {
        case SERIAL_SET_LINE:
        case SERIAL_SET_MODEM:
        case SERIAL_BREAK:
        case SERIAL_EXCHANGE:
        case SERIAL_BIND:
        case SERIAL_UNBIND:
            return true;

        default:
            return false;
    }
}

// Only the file that keeps everyone else out may change the port, so that a sequence
// of requests, such as dialing a modem and binding it, isn't mixed with another's
static bool may_change(const serial_device_t* restrict device)
{
    thread_t* thread = this_thread();

    return chr_device_is_exclusive(&device->chr) && cred_capable(thread->credentials, CAP_SYS_TTY_CONFIG);
}

static int serial_chr_ioctl(const chr_device_t* restrict chr, void* restrict file_ctxdata, uint32_t request, uintptr_t argument)
{
    serial_device_t* device = serial_of(chr);

    if(changes_port(request) && !may_change(device)) {
        return -EPERM;
    }

    switch(request) {
        case SERIAL_GET_INFO:
            return get_info(device, argument);

        case SERIAL_GET_LINE:
            return get_line(device, argument);

        case SERIAL_SET_LINE:
            return set_line(device, argument);

        case SERIAL_GET_MODEM:
            return get_modem(device, argument);

        case SERIAL_SET_MODEM:
            return set_modem(device, argument);

        case SERIAL_BREAK:
            return send_break(device, argument);

        case SERIAL_EXCHANGE:
            return exchange_bytes(device, argument);

        case SERIAL_BIND:
            return bind(device, argument);

        case SERIAL_UNBIND:
            return unbind(device, false);

        case SERIAL_GET_BINDING:
            return get_binding(device, argument);

        default:
            return -ENOTTY;
    }
}

static void serial_chr_release(const chr_device_t* restrict chr)
{
    serial_device_t* device = serial_of(chr);

    mutex_deinit(&device->priv.lock);

    if(device->ops->release) {
        device->ops->release(device);
    }
}

static const chr_device_ops_t serial_chr_ops = {
    .ioctl = &serial_chr_ioctl,
    .release = &serial_chr_release,
};

int serial_device_add(serial_device_t* restrict device)
{
    serial_device_private_t* priv = &device->priv;

    memset(priv, 0, sizeof(*priv));
    mutex_init(&priv->lock);
    priv->dispatch_lock = SPINLOCK_INIT;

    if(device->line.speed == 0) {
        device->line.speed = SERIAL_DEFAULT_SPEED;
        device->line.data_bits = 8;
        device->line.stop_bits = 1;
        device->line.parity = SERIAL_PARITY_NONE;
        device->line.flow = 0;
    }

    int retval = dev_unit_alloc(&dev_prefix_serial, &priv->unit);

    if(retval < 0) {
        mutex_deinit(&priv->lock);
        return retval;
    }

    device->consumer_data = nullptr;
    device->chr.ops = &serial_chr_ops;
    device->chr.ctxdata = device;

    retval = chr_device_add(&device->chr, priv->unit, DEV_NAME_NONE);

    if(retval < 0) {
        dev_unit_free(priv->unit);
        mutex_deinit(&priv->lock);
        return retval;
    }

    bind_from_cmdline(device);

    return 0;
}

int serial_device_remove(serial_device_t* restrict device)
{
    atomic_store(&device->priv.removed, true, ATOMIC_SEQ_CST);

    bool intr_state = spinlock_acquire_intr(&device->priv.dispatch_lock);

    if(device->priv.consumer == &exchange_consumer) {
        exchange_t* exchange = device->consumer_data;
        event_trigger(&exchange->finished, false);
    }

    spinlock_release_intr(&device->priv.dispatch_lock, intr_state);

    // With force, only an unbind already running makes it wait
    while(-EBUSY == unbind(device, true)) {
        sleep_msec(REMOVE_RETRY_MSEC);
    }

    // Waits out the calls into the driver, which see removed from now on
    mutex_acquire(&device->priv.lock);
    shutdown_locked(device);
    mutex_release(&device->priv.lock);

    // The registry's reference may be the last one, so release may come during the removal
    dev_unit_t* unit = device->priv.unit;
    int retval = chr_device_remove(&device->chr);

    if(retval < 0) {
        return retval;
    }

    dev_unit_free(unit);

    return 0;
}

void serial_consumer_register(serial_consumer_t* restrict consumer)
{
    spinlock_acquire(&consumers_lock);
    consumer->next = consumers_head;
    consumers_head = consumer;
    spinlock_release(&consumers_lock);
}

void serial_receive(serial_device_t* restrict device, const void* restrict bytes, size_t count)
{
    bool intr_state = spinlock_acquire_intr(&device->priv.dispatch_lock);
    const serial_consumer_t* consumer = device->priv.consumer;

    if(consumer) {
        consumer->receive(device, bytes, count);
    }
    else {
        device->priv.lost += count;
    }

    spinlock_release_intr(&device->priv.dispatch_lock, intr_state);
}

void serial_receive_error(serial_device_t* restrict device, uint8_t byte, unsigned error)
{
    kassert(error >= SERIAL_ERROR_BREAK && error <= SERIAL_ERROR_OVERRUN);

    bool intr_state = spinlock_acquire_intr(&device->priv.dispatch_lock);
    const serial_consumer_t* consumer = device->priv.consumer;

    if(consumer) {
        consumer->receive_error(device, byte, error);
    }
    else {
        device->priv.lost += 1;
    }

    spinlock_release_intr(&device->priv.dispatch_lock, intr_state);
}

size_t serial_transmit_take(serial_device_t* restrict device, void* restrict buffer, size_t size)
{
    size_t taken = 0;

    bool intr_state = spinlock_acquire_intr(&device->priv.dispatch_lock);
    const serial_consumer_t* consumer = device->priv.consumer;

    if(consumer && size) {
        taken = consumer->transmit_take(device, buffer, size);
    }

    spinlock_release_intr(&device->priv.dispatch_lock, intr_state);

    return taken;
}

void serial_device_detach(serial_device_t* restrict device)
{
    set_dispatch(device, nullptr);
}

int serial_device_startup(serial_device_t* restrict device)
{
    int retval = -ENODEV;

    mutex_acquire(&device->priv.lock);

    if(!is_removed(device)) {
        retval = startup_locked(device);
    }

    mutex_release(&device->priv.lock);

    return retval;
}

void serial_device_shutdown(serial_device_t* restrict device)
{
    mutex_acquire(&device->priv.lock);

    if(!is_removed(device)) {
        shutdown_locked(device);
    }

    mutex_release(&device->priv.lock);
}

int serial_device_set_line(serial_device_t* restrict device, const serial_line_t* restrict line)
{
    int retval = -ENODEV;

    mutex_acquire(&device->priv.lock);

    if(!is_removed(device)) {
        retval = set_line_locked(device, line);
    }

    mutex_release(&device->priv.lock);

    return retval;
}

void serial_device_set_modem(serial_device_t* restrict device, uint32_t raise, uint32_t lower)
{
    mutex_acquire(&device->priv.lock);

    if(!is_removed(device)) {
        set_modem_locked(device, raise, lower);
    }

    mutex_release(&device->priv.lock);
}

int serial_device_send_break(serial_device_t* restrict device, uint32_t msec)
{
    int retval = -ENODEV;

    mutex_acquire(&device->priv.lock);

    if(!is_removed(device)) {
        retval = send_break_locked(device, msec);
    }

    mutex_release(&device->priv.lock);

    return retval;
}

void serial_device_start_tx(serial_device_t* restrict device)
{
    if(!is_removed(device)) {
        device->ops->start_tx(device);
    }
}
