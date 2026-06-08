#include <sys.amd64/tss.h>
#include <sys.arch/hart.h>
#include <sys/page.h>
#include <sys/sched.h>

uintptr_t __used uthread_prepare_switch(struct uthread* restrict next)
{
    struct hart_local* hart = this_hart();
    hart->amd64.tss->rsp[0] = PAGE_SIZE + page_align(next->kstack);
    return next->kstack;
}
