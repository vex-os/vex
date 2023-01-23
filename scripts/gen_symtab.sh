#!/bin/sh
# [1]: elf binary to demangle

# Having an extra printf there allows us to dry-run the script
symbols=$(nm ${1} 2> /dev/null | grep " [TtWw] " | sort | cut -d " " -f "1,3" || printf "")

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"
printf "#include <kan/symbol.h>\n"

printf "const symbol_t __ksymtab[] = {\n"
printf "%s" "${symbols}" | while read -r symbol
do
    printf "{ .address = 0x${symbol%% *}, .name = \"${symbol##* }\" },\n"
done
printf "{ .address = 0x00, .name = { 0 } },\n"
printf "};\n"
