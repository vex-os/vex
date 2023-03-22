#!/bin/sh
printf "c\n" | bochs -q -f ${PWD}/build/x86_64/bochsrc.conf ${@}
printf "\n"
