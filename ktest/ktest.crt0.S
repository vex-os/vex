#include <vex/syscall.h>

.section .text

.globl _start
.type _start, @function

_start:
    movq 0(%rsp), %rdi          # argc
    leaq 8(%rsp), %rsi          # argv
    leaq 8(%rsi,%rdi,8), %rdx   # envp

    call main

    movq %rax, %rdi             # exit code
    movq $__k_SYS_EXIT, %rax
    syscall

.Lspin:
    jmp .Lspin

.section .note.GNU-stack,"",@progbits
