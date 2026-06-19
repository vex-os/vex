.section .text

# Standalone freestanding ET_EXEC userland test binary, loaded through the
# real elf64 binfmt (exec/elf64.c) instead of being hand-mapped as a raw
# kernel blob (the old arch/amd64/proc/utest.S approach). Built and linked
# separately from the kernel itself: see boot/utest/utest.lds and the
# UTEST_ELF rules in GNUmakefile.boot.
#
# Passes a fixed (ptr, sz) pair pointing at the message constant below to
# SYS_TEST, which copies it out of user memory and logs it, then exits via
# SYS_EXIT.
.global _start
.type _start, @function
_start:
    movq $message, %rdi     # ptr
    movq $message_len, %rsi # sz
    movq $32, %rax          # SYS_TEST
    syscall

    # Deliberate #DE (divide error): the kernel currently has no handler for
    # this vector (arch/amd64/ireq/isr.c only handles #PF) and falls through
    # to panic() for anything else, so this takes the whole kernel down
    # instead of just this process. Left in on purpose to demonstrate the
    # gap, not a real userland fault test.
    xorl %edx, %edx
    xorl %eax, %eax
    xorl %ecx, %ecx
    divl %ecx

    movq $1, %rax          # SYS_EXIT
    syscall

.Lspin:                    # should be unreachable: SYS_EXIT doesn't return
    jmp .Lspin

.section .rodata

message:
    .ascii "a big test message for utest.S running in userland!!!"
message_len = . - message
