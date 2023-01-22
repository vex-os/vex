#!/bin/sh
bochs -q -f ${PWD}/scripts/x86/bochsrc ${@}
printf "\n"
