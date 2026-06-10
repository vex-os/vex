.section .text

.global x86_thread_trampoline_kern
.type x86_thread_trampoline_kern, @function
x86_thread_trampoline_kern:
    sti
    movq %r13, %rdi
    callq *%r12
    movq %rax, %rdi
    callq thread_exit_handler

.global x86_thread_trampoline_user
.type x86_thread_trampoline_user, @function
x86_thread_trampoline_user:
    swapgs
    iretq

.global thread_switch
.type thread_switch, @function
thread_switch:
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
