#include <sys.amd64/tss.h>
#include <sys.arch/hart.h>
#include <sys/page.h>
#include <sys/process.h>
#include <sys/sched.h>
#include <sys/vmm.h>

uintptr_t __used uthread_prepare_switch(struct uthread* restrict next)
{
    struct hart_local* hart = this_hart();

    uintptr_t kstack_top = PAGE_SIZE + page_align(next->kstack);
    hart->amd64_tssp->rsp[0] = kstack_top;
    hart->amd64_syscall_ksp = kstack_top;

    vmm_switch(next->owner->vm);

    return next->kstack;
}
