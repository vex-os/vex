## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c), 2022, Kaneru Contributors

AS		:= clang -pipe
CC		:= clang -pipe
LD		:= ld.lld
OBJDUMP	:= llvm-objdump

ASFLAGS	:=
CCFLAGS	:=
CPFLAGS	:=
LDFLAGS	:=

%.c.o %.o: %.c
	$(CC) $(CCFLAGS) $(CPFLAGS) -c -o $@ $<

%.S.o %.o: %.S
	$(AS) $(ASFLAGS) $(CPFLAGS) -c -o $@ $<

%.s.o %.o: %.s
	$(AS) $(ASFLAGS) $(CPFLAGS) -c -o $@ $<

%.ld: %.lds
	$(CC) $(CPFLAGS) -D__ASSEMBLER__ -E -xc $< | $(GREP) -v "^#" > $@ || $(TRUE)
