#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/cmdline.h>
#include <vex/console.h>
#include <vex/errno.h>
#include <vex/kprintf.h>
#include <vex/panic.h>
#include <vex/spinlock.h>
#include <vex/sprintf.h>
#include <vex/string.h>
#include <vex/strings.h>

#define TIMESTAMP_MAX   24U // Fits "[%5ju.%04ju] " for any uint64_t of nanoseconds
#define OUTPUT_LINE_MAX (TIMESTAMP_MAX + KP_LINE_MAX + 2U)

static spinlock_t console_lock = SPINLOCK_INIT;
static console_t* console_head = nullptr;

// Under console_lock; a panic overwrites them
// but the write they were for is never finished
static char record_text[KP_LINE_MAX];
static char output_line[OUTPUT_LINE_MAX];

static unsigned console_loglevel(const console_t* restrict cons)
{
    if(cons->consflags & CONSF_LOGLEVEL) {
        return cons->loglevel;
    }

    return loglevel;
}

static size_t format_record(const kp_record_t* restrict record)
{
    uint64_t seconds = record->time_ns / UINT64_C(1000000000);
    uint64_t fraction = (record->time_ns / UINT64_C(100000)) % UINT64_C(10000);
    int length = snprintf(output_line, TIMESTAMP_MAX, "[%5ju.%04ju] ", (uintmax_t)(seconds), (uintmax_t)(fraction));
    size_t size = (size_t)(length);

    memcpy(&output_line[size], record_text, record->size);
    size += record->size;

    output_line[size] = '\r';
    output_line[size + 1U] = '\n';

    return size + 2U;
}

static void write_dropped(console_t* restrict cons, uint64_t count)
{
    int length = snprintf(output_line, sizeof(output_line), "** %ju messages dropped **\r\n", (uintmax_t)(count));
    cons->ops->write(cons, output_line, (size_t)(length));
}

static void flush_console(console_t* restrict cons)
{
    uint64_t expected = cons->priv.cursor.seq;
    kp_record_t record;

    while(kprintf_read(&cons->priv.cursor, &record, record_text)) {
        if(record.seq != expected) {
            write_dropped(cons, record.seq - expected);
        }

        expected = record.seq + 1U;

        if(record.level <= console_loglevel(cons)) {
            cons->ops->write(cons, output_line, format_record(&record));
        }
    }
}

static void flush_all_locked(void)
{
    for(console_t* it = console_head; it; it = it->priv.next) {
        if(it->ops->write == nullptr)
            continue;
        if(it->consflags & CONSF_RELEASED)
            continue;
        flush_console(it);
    }
}

int console_add(console_t* restrict cons)
{
    bool irq_state = spinlock_acquire_intr(&console_lock);

    for(console_t* it = console_head; it; it = it->priv.next) {
        if(it == cons) {
            spinlock_release_intr(&console_lock, irq_state);
            return -EEXIST;
        }
    }

    spinlock_release_intr(&console_lock, irq_state);

    pr_verbose("console: adding %s", cons->name);

    if(cons->ops->init) {
        cons->ops->init(cons);
    }

    irq_state = spinlock_acquire_intr(&console_lock);

    if(cons->consflags & CONSF_RECALL) {
        kprintf_seek_first(&cons->priv.cursor);
    }
    else {
        kprintf_seek_end(&cons->priv.cursor);
    }

    cons->priv.next = console_head;
    console_head = cons;

    spinlock_release_intr(&console_lock, irq_state);

    console_flush();

    return 0;
}

int console_remove(console_t* restrict cons)
{
    bool irq_state = spinlock_acquire_intr(&console_lock);

    if(console_head == cons) {
        console_head = console_head->priv.next;

        spinlock_release_intr(&console_lock, irq_state);

        pr_verbose("console: removing %s", cons->name);

        return 0;
    }

    console_t* prev = nullptr;

    for(console_t* it = console_head; it; it = it->priv.next) {
        if(it == cons) {
            if(prev) {
                prev->priv.next = it->priv.next;
            }

            spinlock_release_intr(&console_lock, irq_state);

            pr_verbose("console: removing %s", cons->name);

            return 0;
        }

        prev = it;
    }

    spinlock_release_intr(&console_lock, irq_state);

    return -EINVAL;
}

void console_flush(void)
{
    bool irq_state = disable_interrupts();

    // A panic has stopped the other harts, so a
    // holder of the lock is either stopped or this hart
    if(atomic_load_n(&panic_in_progress, ATOMIC_ACQUIRE)) {
        flush_all_locked();
        restore_interrupts(irq_state);
        return;
    }

    for(;;) {
        // Either the writer that holds the lock sees the record appended
        // before this fence, or this hart takes the lock and writes it
        atomic_thread_fence(ATOMIC_SEQ_CST);

        if(!raw_spinlock_try_acquire(&console_lock)) {
            break;
        }

        uint64_t written = kprintf_next_seq();

        flush_all_locked();

        raw_spinlock_release(&console_lock);

        // Pairs with the fence above on the hart that failed to take the lock
        atomic_thread_fence(ATOMIC_SEQ_CST);

        if(kprintf_next_seq() == written) {
            break;
        }
    }

    restore_interrupts(irq_state);
}

unsigned console_lookup(const char* restrict name, char* restrict options, size_t size)
{
    const char* list = cmdline_argument(&sys_cmdline, "console");
    size_t name_length = strlen(name);

    options[0] = 0;

    if(list == nullptr) {
        return CONSOLE_UNSPECIFIED;
    }

    while(*list) {
        const char* comma = strchr(list, ',');
        size_t length = strlen(list);

        if(comma) {
            length = (size_t)(comma - list);
        }

        bool matches = length >= name_length && 0 == strncmp(list, name, name_length);
        bool has_options = length > name_length && list[name_length] == ':';

        if(matches && length == name_length) {
            return CONSOLE_LISTED;
        }

        if(matches && has_options) {
            size_t copied = length - name_length - 1;

            if(copied >= size) {
                copied = size - 1;
            }

            kstrncpy(options, &list[name_length + 1], copied);
            options[copied] = 0;

            return CONSOLE_LISTED;
        }

        list += comma ? length + 1 : length;
    }

    return CONSOLE_UNLISTED;
}

void console_release(console_t* restrict cons)
{
    bool irq_state = spinlock_acquire_intr(&console_lock);

    if(0 == (cons->consflags & CONSF_RELEASED)) {
        if(cons->ops->release) {
            cons->ops->release(cons);
        }

        cons->consflags |= CONSF_RELEASED;
    }

    spinlock_release_intr(&console_lock, irq_state);
}

void console_takeover_all(void)
{
    for(console_t* it = console_head; it; it = it->priv.next) {
        if(0 == (it->consflags & CONSF_RELEASED))
            continue;
        if(it->ops->takeover && !it->ops->takeover(it))
            continue;

        it->consflags &= ~CONSF_RELEASED;

        if(it->consflags & CONSF_RECALL) {
            kprintf_seek_first(&it->priv.cursor);
        }
        else {
            kprintf_seek_end(&it->priv.cursor);
        }
    }

    console_flush();
}
