.section .text

#define HART_SYSCALL_KSP 0x08   // hart_local::amd64_syscall_ksp
#define HART_SYSCALL_USP 0x10   // hart_local::amd64_syscall_usp

.global amd64_syscall_entry
.type amd64_syscall_entry, @function
amd64_syscall_entry:
    swapgs
    movq %rsp, %gs:HART_SYSCALL_USP
    movq %gs:HART_SYSCALL_KSP, %rsp

    // save ksp so we can restore it after dispatch (scheduler may clobber it)
    pushq %rsp

    // sysretq needs rcx (user RIP) and r11 (user RFLAGS) -- save before call clobbers them
    pushq %rcx
    pushq %r11

    subq $0x08, %rsp    // syscall_frame::retval
    pushq %r9           // syscall_frame::arg_5
    pushq %r8           // syscall_frame::arg_4
    pushq %r10          // syscall_frame::arg_3
    pushq %rdx          // syscall_frame::arg_2
    pushq %rsi          // syscall_frame::arg_1
    pushq %rdi          // syscall_frame::arg_0
    pushq %rax          // syscall_frame::number

    movq %rsp, %rdi
    call dispatch_syscall

    movq 56(%rsp), %rax // load retval for userspace
    addq $64, %rsp      // pop entire frame

    popq %r11           // restore user RFLAGS
    popq %rcx           // restore user RIP
    popq %rax           // restore saved ksp
    movq %rax, %gs:HART_SYSCALL_KSP

    movq %gs:HART_SYSCALL_USP, %rsp
    swapgs
    sysretq
