.section .text

.global sched_enter
.type sched_enter, @function
sched_enter:
    movq %rdi, %rsp     # hart_context at rdi

    # No longer on the previous thread's stack,
    # so another hart may now resume that thread
    testq %rsi, %rsi
    jz 2f
    movl $0, (%rsi)     # SPINLOCK_FREE
2:
    popq %rax
    movq %rax, %ds      # hart_context::amd64_ds
    popq %rax
    movq %rax, %es      # hart_context::amd64_es
    popq %rax           # hart_context::amd64_rax
    popq %rbx           # hart_context::amd64_rbx
    popq %rcx           # hart_context::amd64_rcx
    popq %rdx           # hart_context::amd64_rdx
    popq %rsi           # hart_context::amd64_rsi
    popq %rdi           # hart_context::amd64_rdi
    popq %rbp           # hart_context::amd64_rbp
    popq %r8            # hart_context::amd64_r8
    popq %r9            # hart_context::amd64_r9
    popq %r10           # hart_context::amd64_r10
    popq %r11           # hart_context::amd64_r11
    popq %r12           # hart_context::amd64_r12
    popq %r13           # hart_context::amd64_r13
    popq %r14           # hart_context::amd64_r14
    popq %r15           # hart_context::amd64_r15
    addq $0x08, %rsp    # hart_context::amd64_err

    testq $3, 8(%rsp)   # CS is at 8(%rsp); test RPL bits for ring 3
    jz 1f
    swapgs
1:  iretq
