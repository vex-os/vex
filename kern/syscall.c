#include <string.h>
#include <sys.arch/hart.h>
#include <sys/assert.h>
#include <sys/errno.h>
#include <sys/initcall.h>
#include <sys/kprintf.h>
#include <sys/malloc.h>
#include <sys/panic.h>
#include <sys/signal.h>
#include <sys/syscall.h>
#include <sys/thread.h>

static struct syscall* syscall_table;
static size_t syscall_table_size;

void dispatch_syscall(struct regfile* restrict frame)
{
    if(frame->syscall_number >= syscall_table_size) {
        frame->syscall_retval = -ENOSYS;
        return;
    }

    if(syscall_table[frame->syscall_number].function == nullptr) {
        frame->syscall_retval = -ENOSYS;
        return;
    }

    syscall_table[frame->syscall_number].function(frame);

    struct hart_local* hart = this_hart();
    struct thread* thread = hart->curr_thread;

    if(thread->owner && signal_pending(thread)) {
        signal_deliver(thread, frame);
    }
}

static void init_syscall(void)
{
    if(&unsorted_syscalls_start[0] == &unsorted_syscalls_end[0]) {
        pr_alert("syscall: no system calls defined?!");
        return;
    }

    unsigned long max_number = 0;

    for(const struct syscall* it = unsorted_syscalls_start; it < unsorted_syscalls_end; ++it) {
        if(it->number > max_number) {
            max_number = it->number;
        }
    }

    syscall_table_size = max_number + 1;
    syscall_table = kmalloc(syscall_table_size * sizeof(struct syscall));

    if(syscall_table == nullptr) {
        panic("syscall: out of memory");
    }

    for(const struct syscall* it = unsorted_syscalls_start; it < unsorted_syscalls_end; ++it) {
        kassert(syscall_table[it->number].function == nullptr);
        kassert(syscall_table[it->number].identifier == nullptr);

        memcpy(&syscall_table[it->number], it, sizeof(struct syscall));
    }
}

inticall_bsp_syscall(init_syscall, INIT_FIRST);
