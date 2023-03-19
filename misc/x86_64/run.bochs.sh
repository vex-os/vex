#!/bin/sh
printf "c\n" | bochs -q -f ${PWD}/misc/x86_64/bochsrc.conf ${@}
printf "\n"
