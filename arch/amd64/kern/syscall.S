.section .text

#define HART_SYSCALL_KSP 0x08 // hart_local::amd64_syscall_ksp
#define HART_SYSCALL_USP 0x10 // hart_local::amd64_syscall_usp

#define SEL_USER_CS 0x43 // gdt_selector(GDT_USER_CODE_64, 0, 3)
#define SEL_USER_SS 0x3B // gdt_selector(GDT_USER_DATA_64, 0, 3)

#define REGFILE_RAX     0x00 // regfile::amd64_rax
#define REGFILE_RIP     0x80 // regfile::amd64_rip
#define REGFILE_RFLAGS  0x90 // regfile::amd64_rflags
#define REGFILE_RSP     0x98 // regfile::amd64_rsp

.global amd64_syscall_entry
.type amd64_syscall_entry, @function
amd64_syscall_entry:
    swapgs
    movq %rsp, %gs:HART_SYSCALL_USP
    movq %gs:HART_SYSCALL_KSP, %rsp

    pushq $SEL_USER_SS          // regfile::amd64_ss
    subq  $0x08, %rsp           // regfile::amd64_rsp
    pushq %r11                  // regfile::amd64_rflags    -> r11 in syscall
    pushq $SEL_USER_CS          // regfile::amd64_cs
    pushq %rcx                  // regfile::amd64_rip       -> rcx in syscall
    pushq $0x0000000000000000   // regfile::amd64_err
    pushq %r15                  // regfile::amd64_r15
    pushq %r14                  // regfile::amd64_r14
    pushq %r13                  // regfile::amd64_r13
    pushq %r12                  // regfile::amd64_r12
    pushq %r11                  // regfile::amd64_r11
    pushq %r10                  // regfile::amd64_r10
    pushq %r9                   // regfile::amd64_r9
    pushq %r8                   // regfile::amd64_r8
    pushq %rbp                  // regfile::amd64_rbp
    pushq %rdi                  // regfile::amd64_rdi
    pushq %rsi                  // regfile::amd64_rsi
    pushq %rdx                  // regfile::amd64_rdx
    pushq %rcx                  // regfile::amd64_rcx
    pushq %rbx                  // regfile::amd64_rbx
    pushq %rax                  // regfile::amd64_rax

    movq %gs:HART_SYSCALL_USP, %rbx
    movq %rbx, REGFILE_RSP(%rsp)

    movq %rsp, %rdi
    call dispatch_syscall

    movq REGFILE_RAX(%rsp), %rax    // retval
    movq REGFILE_RIP(%rsp), %rcx    // user rip, rcx for sysretq
    movq REGFILE_RFLAGS(%rsp), %r11 // user rflags, r11 for sysretq
    movq REGFILE_RSP(%rsp), %rsp

    swapgs
    sysretq
