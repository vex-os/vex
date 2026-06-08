#include <sys.amd64/tss.h>
#include <sys.arch/hart.h>
#include <sys/page.h>
#include <sys/process.h>
#include <sys/sched.h>
#include <sys/vmm.h>

uintptr_t __used uthread_prepare_switch(struct uthread* restrict next)
{
    struct hart_local* hart = this_hart();
    hart->amd64.tss->rsp[0] = PAGE_SIZE + page_align(next->kstack);
    vmm_switch(next->owner->vm);
    return next->kstack;
}
