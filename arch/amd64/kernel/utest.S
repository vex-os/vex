.section .text

# Stand-in for a real ELF payload until there's an actual loader: reuses
# the kernel's own compiled code as the ring-3 test program. .balign before
# *and* after means this routine has the page entirely to itself, so the
# caller (kmain.c) can map exactly its physical page with VPROT_URX without
# accidentally exposing anything else.
#
# Reads argc/argv[0] straight off the entry stack (the System V AMD64
# process-entry layout built by build_user_argv_stack(), kernel/sched.c),
# round-trips them through SYS_TEST so the kernel can print back what it
# read from user memory, then exits via SYS_EXIT.
.balign 4096
.global amd64_utest_blob
.type amd64_utest_blob, @function
amd64_utest_blob:
    movq (%rsp), %rdi      # argc
    movq 8(%rsp), %rsi     # argv[0]
    movq $2, %rax          # SYS_TEST
    syscall

    movq $1, %rax          # SYS_EXIT
    syscall

.Lspin:                    # should be unreachable: SYS_EXIT doesn't return
    jmp .Lspin
.balign 4096
