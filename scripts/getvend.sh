#!/bin/sh

vendors=""
vendors="${vendors} demos-elf"
vendors="${vendors} kaneru-elf"
vendors="${vendors} unknown-elf"

arch="${1:-x86_64}"

for vendor in ${vendors}; do
    if command -v "${arch}-${vendor}-cc" &> /dev/null; then
        printf "${vendor}\n"
        exit 0
    fi
done

exit 1
