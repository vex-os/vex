#include <vex/atomic.h>
#include <vex/sched.h>
#include <vex/signal.h>
#include <vex/thread.h>

void exit_to_user(hart_context_t* restrict ctx)
{
    thread_t* thread = this_thread();

    for(;;) {
        uint32_t work = atomic_load(&thread->pending_work, ATOMIC_ACQUIRE);

        if(work == 0) {
            return;
        }

        enable_interrupts();

        if(work & THREAD_WORK_EXIT) {
            sched_terminate();
            unreachable();
        }

        if(work & THREAD_WORK_SIGNAL) {
            signal_deliver(ctx);
        }

        disable_interrupts();
    }
}
