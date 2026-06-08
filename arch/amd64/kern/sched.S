.section .text

.global amd64_halt_loop
.type amd64_halt_loop, @function
amd64_halt_loop:
    hlt
    jmp amd64_halt_loop

.global amd64_kthread_trampoline
.type amd64_kthread_trampoline, @function
amd64_kthread_trampoline:
    sti
    movq %r13, %rdi
    callq *%r12
    movq %rax, %rdi
    callq kthread_exit_handler

.global task_switch_kern
.type task_switch_kern, @function
task_switch_kern:
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

.global task_switch_user
.type task_switch_user, @function
task_switch_user:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, 0(%rdi)
    movq %rsi, %rdi
    callq uthread_prepare_switch

    movq %rax, %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    iretq

.global task_enter_user
.type task_enter_user, @function
task_enter_user:
    callq uthread_prepare_switch

    movq %rax, %rsp

    iretq
