#!/bin/sh

vectors=$(seq 0 255)

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"

printf ".macro save_frame\n"
printf "pushq %%rax; pushq %%rcx; pushq %%rdx; pushq %%rbx; pushq %%rbp; pushq %%rsi; pushq %%rdi;\n"
printf "pushq %%r8; pushq %%r9; pushq %%r10; pushq %%r11; pushq %%r12; pushq %%r13; pushq %%r14; pushq %%r15;\n"
printf ".endm\n"

printf ".macro restore_frame\n"
printf "popq %%r15; popq %%r14; popq %%r13; popq %%r12; popq %%r11; popq %%r10; popq %%r9; popq %%r8\n"
printf "popq %%rdi; popq %%rsi; popq %%rbp; popq %%rbx; popq %%rdx; popq %%rcx; popq %%rax;\n"
printf ".endm\n"

printf "\n"
printf ".section .text\n"

for i in ${vectors}; do
    printf "__isr_${i}:\n"

    # Certain interrupt vectors on x86_64 push an error
    # code alongside some parts of x86_frame, so we have
    # to account for other interrupts by pushing a stub
    if [ ${i} -ne 8 ] && [ ${i} -lt 10 ] || [ ${i} -gt 14 ] && [ ${i} -ne 21 ]; then
        printf "pushq \$0\n"
    fi

    # Save CPU context
    printf "save_frame\n"

    # Call a C function:
    # void isr_handler(cpu_ctx_t *, uint64_t)
    printf "movq %%rsp, %%rdi\n"
    printf "movq \$${i}, %%rsi\n"
    printf "call isr_handler\n"

    # Restore CPU context
    printf "restore_frame\n"

    # Discard the error code
    printf "addq \$0x08, %%rsp\n"

    # Return from interrupt
    printf "sti\n"
    printf "iretq\n"
done

printf ".section .rodata\n"
printf ".global isr_stubs\n"
printf ".type isr_stubs, @object\n"
printf "isr_stubs:\n"
for i in ${vectors}; do
    printf ".quad __isr_${i}\n"
done
