.section .text

.global sigreturn_guard
.type sigreturn_guard, @function
.align 4096
sigreturn_guard:
    movq $0x01, %rax # SYS_EXIT(255)
    movq $0xFF, %rdi
    syscall
