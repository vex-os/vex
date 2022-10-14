#!/bin/bash

objdump="$1"
kernel_noinit="$2"
initcalls=$($objdump -j ".discard.init" -s $kernel_noinit | grep "^[ \t]*[0-9a-f]" | xxd -r | tr "\0" "\n" | tsort)

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"
printf "#include <stddef.h>\n"
printf "#include <psys/initcall.h>\n"

for initcall in $initcalls; do
    printf "initcall_extern($initcall);\n"
done

printf "const initcall_t __initcalls[] = {\n"
for initcall in $initcalls; do
    printf "__init_$initcall,\n"
done
printf "NULL\n"
printf "};\n"
