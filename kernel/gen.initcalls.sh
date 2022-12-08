#!/bin/bash

# we want the symbol list to be a bash array of strings
# separated by a newline, so we have to explicitly state that
# elements within that array are separated, well... by a newline.
IFS=$'\n'

# gather and dependency-sort the list of initcalls
initcalls=($(${1} -j ".discard.init" -s "${2}" | grep "^[ \t]*[0-9a-f]" | xxd -r | tr "\0" "\n" | tsort))

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"
printf "#include <kaneru/initcall.h>\n"
printf "#include <stddef.h>\n"

# declare initcall functions
for initcall in ${initcalls[@]}; do
    printf "extern int __init_${initcall}(void);\n"
done

printf "const struct initcall __initcalls[] = {\n"
for initcall in ${initcalls[@]}; do
    printf "{ .func = &__init_${initcall}, .name = \"${initcall}\" },\n"
done
printf "{ .func = NULL, .name = NULL }\n"
printf "};\n"
