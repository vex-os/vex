.section .text

.global amd64_syscall_entry
.type amd64_syscall_entry, @function
amd64_syscall_entry:
    swapgs                      # %gs:0                         -> thread::self

    movq %rsp, %gs:24           # thread::entry_scratch         <- user rsp for later sysretq
    movq %gs:16, %rsp           # thread::kernel_rsp

    pushq $0x3B                 # hart_context::amd64_ss        <- gdt_selector(GDT_USER_DATA_64, 0, 3)
    pushq %gs:24                # hart_context::amd64_rsp
    pushq %r11                  # hart_context::amd64_rflags
    pushq $0x43                 # hart_context::amd64_cs        <- gdt_selector(GDT_USER_CODE_64, 0, 3)
    pushq %rcx                  # hart_context::amd64_rip
    pushq $0x0000000000000000   # hart_context::amd64_err
    pushq %r15                  # hart_context::amd64_r15
    pushq %r14                  # hart_context::amd64_r14
    pushq %r13                  # hart_context::amd64_r13
    pushq %r12                  # hart_context::amd64_r12
    pushq %r11                  # hart_context::amd64_r11
    pushq %r10                  # hart_context::amd64_r10
    pushq %r9                   # hart_context::amd64_r9
    pushq %r8                   # hart_context::amd64_r8
    pushq %rbp                  # hart_context::amd64_rbp
    pushq %rdi                  # hart_context::amd64_rdi
    pushq %rsi                  # hart_context::amd64_rsi
    pushq %rdx                  # hart_context::amd64_rdx
    pushq %rcx                  # hart_context::amd64_rcx
    pushq %rbx                  # hart_context::amd64_rbx
    pushq %rax                  # hart_context::amd64_rax
    movq %es, %rax
    pushq %rax                  # hart_context::amd64_es
    movq %ds, %rax
    pushq %rax                  # hart_context::amd64_ds

    sti

    movq %rsp, %rdi
    call dispatch_syscall

    cli

    popq %rax                   # hart_context::amd64_ds
    movq %rax, %ds
    popq %rax                   # hart_context::amd64_es
    movq %rax, %es
    popq %rax                   # hart_context::amd64_rax
    popq %rbx                   # hart_context::amd64_rbx
    popq %rcx                   # hart_context::amd64_rcx
    popq %rdx                   # hart_context::amd64_rdx
    popq %rsi                   # hart_context::amd64_rsi
    popq %rdi                   # hart_context::amd64_rdi
    popq %rbp                   # hart_context::amd64_rbp
    popq %r8                    # hart_context::amd64_r8
    popq %r9                    # hart_context::amd64_r9
    popq %r10                   # hart_context::amd64_r10
    popq %r11                   # hart_context::amd64_r11
    popq %r12                   # hart_context::amd64_r12
    popq %r13                   # hart_context::amd64_r13
    popq %r14                   # hart_context::amd64_r14
    popq %r15                   # hart_context::amd64_r15

    movq %gs:24, %rsp           # thread::entry_scratch         <- restore user rsp for sysretq

    swapgs                      # %gs:0                         -> whatever userspace had

    # SYSRET with a non-canonical RCX faults in ring 0 on Intel,
    # so whatever rewrites amd64_rip has to keep it in the user half,
    # as exec does by rejecting such entry points
    sysretq
