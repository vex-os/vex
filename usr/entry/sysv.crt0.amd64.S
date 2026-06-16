.section .text

.globl _start
.type _start, @function

_start:
    movq 0(%rsp), %rdi          # argc
    leaq 8(%rsp), %rsi          # argv
    leaq 8(%rsi,%rdi,8), %rdx   # envp

    call main

    movq %rax, %rdi # exit code
    movq $1, %rax   # __k_SYS_EXIT (include/usr/vex/syscall.h)
    syscall

.Lspin:
    jmp .Lspin
