.section .text

.global amd64_thread_trampoline_kern
.type amd64_thread_trampoline_kern, @function
amd64_thread_trampoline_kern:
    sti
    movq %r13, %rdi
    callq *%r12
    movq %rax, %rdi
    callq thread_exit_handler

.global amd64_thread_trampoline_user
.type amd64_thread_trampoline_user, @function
amd64_thread_trampoline_user:
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
