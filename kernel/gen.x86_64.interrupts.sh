#!/bin/sh

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"
printf ".section .text\n"
for i in $(seq 0 255); do
    printf "interrupt_$i:\n"

    # These interrupts appear to "misbehave" and
    # have an error code pushed before we can do
    # anything. Hence others must do the same thing.
    (( i != 8 && ( i < 10 || i > 14 ) && i != 21 )) && \
        printf "pushq \$0\n"

    printf "pushq \$$i\n"
    printf "jmp __interrupt_trampoline\n"
done
printf ".section .rodata\n"
printf ".global __interrupt_stubs\n"
printf "__interrupt_stubs:\n"
for i in $(seq 0 255); do
printf ".quad interrupt_$i\n"
done
