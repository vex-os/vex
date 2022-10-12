## SPDX-License-Identifier: GPL-2.0
## Copyright (c), 2022, Kaneru Contributors

ARFLAGS :=
ASFLAGS :=
CCFLAGS :=
CPFLAGS :=
LDFLAGS :=

PRINTF_PREFIX ?= unknown
PRINTF := printf "[$(PRINTF_PREFIX)] %s\n"

CP := cp
GREP := grep
MKDIR := mkdir -p
TRUE := true
RSYNC := rsync
RM := rm -f

AS := $(TARGET)-$(VENDOR)-gcc
CC := $(TARGET)-$(VENDOR)-gcc
LD := $(TARGET)-$(VENDOR)-gcc -g
OD := $(TARGET)-$(VENDOR)-objdump

%.S.o %.o: %.S
	@$(PRINTF) "assembling $<"
	@$(AS) $(ASFLAGS) $(ASFLAGS.$*.S) $(CPFLAGS) -c -o $@ $(realpath $<)

%.c.o %.o: %.c
	@$(PRINTF) "compiling $<"
	@$(CC) $(CCFLAGS) $(CCFLAGS.$*.c) $(CPFLAGS) -c -o $@ $(realpath $<)

%.ld: %.lds
	@$(PRINTF) "generating $@"
	@$(CC) $(CCFLAGS) $(CPFLAGS) -D__ASSEMBLER__=1 -E -xc $(realpath $<) | $(GREP) -v "^#" > $@ || $(TRUE)
