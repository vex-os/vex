## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c), 2022, Kaneru Contributors

## Default options
ASFLAGS	:=
CCFLAGS	:=
CPFLAGS	:=
LDFLAGS	:=

## UNIX utilities
CP		:= cp
GREP	:= grep
MKDIR	:= mkdir -p
TRUE	:= true
RM		:= rm -f
RMV		:= rm -fv

## Logging via printf
LOG_PFX	?= unknown
LOG		:= printf "[$(LOG_PFX)] %s\n"

## Clang rock band
CLANG	:= clang
LLD		:= ld.lld
OBJDUMP	:= llvm-objdump

## Common recipes
%.c.o %.o: %.c
	@$(LOG) "compiling $<"
	@$(CLANG) $(CCFLAGS) $(CPFLAGS) -c -o $@ $<
%.S.o %.o: %.S
	@$(LOG) "assembling $<"
	@$(CLANG) $(ASFLAGS) $(CPFLAGS) -c -o $@ $<

## Preprocess .lds files
%.ld: %.lds
	@$(LOG) "generating $@"
	@$(CLANG) $(CPFLAGS) -D__ASSEMBLER__ -E -xc $< | $(GREP) -v "^#" > $@ || $(TRUE)
