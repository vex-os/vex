#!/bin/sh
bochs -q -f ${PWD}/scripts/x86_64/bochsrc.conf ${@}
printf "\n"
