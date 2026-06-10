.section .text

.set HART_SYSCALL_KSP, 0x08 # hart_local::x86_syscall_ksp
.set HART_SYSCALL_USP, 0x10 # hart_local::x86_syscall_usp

.set SEL_USER_CS, 0x43 # gdt_selector(GDT_USER_CODE_64, 0, 3)
.set SEL_USER_SS, 0x3B # gdt_selector(GDT_USER_DATA_64, 0, 3)

.set REGFILE_RAX,       0x00 # regfile::x86_rax
.set REGFILE_RIP,       0x80 # regfile::x86_rip
.set REGFILE_RFLAGS,    0x90 # regfile::x86_rflags
.set REGFILE_RSP,       0x98 # regfile::x86_rsp

.global x86_syscall_entry
.type x86_syscall_entry, @function
x86_syscall_entry:
    swapgs
    movq %rsp, %gs:HART_SYSCALL_USP
    movq %gs:HART_SYSCALL_KSP, %rsp

    pushq $SEL_USER_SS          # regfile::x86_ss
    subq  $0x08, %rsp           # regfile::x86_rsp
    pushq %r11                  # regfile::x86_rflags    -> r11 in syscall
    pushq $SEL_USER_CS          # regfile::x86_cs
    pushq %rcx                  # regfile::x86_rip       -> rcx in syscall
    pushq $0x0000000000000000   # regfile::x86_err
    pushq %r15                  # regfile::x86_r15
    pushq %r14                  # regfile::x86_r14
    pushq %r13                  # regfile::x86_r13
    pushq %r12                  # regfile::x86_r12
    pushq %r11                  # regfile::x86_r11
    pushq %r10                  # regfile::x86_r10
    pushq %r9                   # regfile::x86_r9
    pushq %r8                   # regfile::x86_r8
    pushq %rbp                  # regfile::x86_rbp
    pushq %rdi                  # regfile::x86_rdi
    pushq %rsi                  # regfile::x86_rsi
    pushq %rdx                  # regfile::x86_rdx
    pushq %rcx                  # regfile::x86_rcx
    pushq %rbx                  # regfile::x86_rbx
    pushq %rax                  # regfile::x86_rax

    movq %gs:HART_SYSCALL_USP, %rbx
    movq %rbx, REGFILE_RSP(%rsp)

    movq %rsp, %rdi
    call dispatch_syscall

    movq REGFILE_RAX(%rsp), %rax    # retval
    movq REGFILE_RIP(%rsp), %rcx    # user rip, rcx for sysretq
    movq REGFILE_RFLAGS(%rsp), %r11 # user rflags, r11 for sysretq

    # Disable interrupts before switching to user RSP: a timer interrupt firing
    # between the RSP switch and sysretq would run on the user stack and cause
    # sched_preempt to save user RSP as the kernel_stack pointer, corrupting the
    # thread's resume path. sysretq restores RFLAGS from r11 (user flags, IF=1),
    # so interrupts are re-enabled transparently upon return to user space.
    cli
    movq REGFILE_RSP(%rsp), %rsp

    swapgs
    sysretq
