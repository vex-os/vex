#!/bin/sh

initcalls=$(${1} -j ".discard.initcall.deps" -s "${2}" | grep "^[ \t]*[0-9a-f]" | xxd -r | tr "\0" "\n" | tsort 2> /dev/null)

echo ".section .data"
echo ".global __initcalls"
echo "__initcalls:"
for i in ${initcalls}; do
    echo "    .quad __initcall_${i}"
done
echo "    .quad 0"

exit 0
