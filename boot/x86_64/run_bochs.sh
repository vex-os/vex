#!/bin/sh
printf "c\n" | bochs -q -f ${PWD}/boot/x86_64/bochsrc.conf ${@}
printf "\n"
