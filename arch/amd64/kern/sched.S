.section .text

.global kthread_trampoline
.type kthread_trampoline, @function
kthread_trampoline:
    sti
    movq %r13, %rdi
    callq *%r12
    movq %rax, %rdi
    callq kthread_exit_handler

.global kthread_switch
.type kthread_switch, @function
kthread_switch:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, 0(%rdi)
    movq 0(%rsi), %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    ret
