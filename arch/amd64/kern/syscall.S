.section .text

.global amd64_syscall_entry
.type amd64_syscall_entry, @function
amd64_syscall_entry:
    movq %rsp, %gs:16
    movq %gs:8, %rsp

    pushq %rcx
    pushq %r11

    pushq $0
    pushq %r9
    pushq %r8
    pushq %r10
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rax

    movq %rsp, %rdi
    call dispatch_syscall

    movq 56(%rsp), %rax
    addq $64, %rsp

    popq %r11
    popq %rcx

    movq %gs:16, %rsp
    sysretq
