.section .text

.global amd64_task_trampoline
.type amd64_task_trampoline, @function
amd64_task_trampoline:
    sti
    jmpq *%rbp

.global sched_switch
.type sched_switch, @function
sched_switch:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, 8(%rdi)
    movq 8(%rsi), %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    ret
