## SPDX-License-Identifier: GPL-2.0-only
## Copyright (c), 2022, Kaneru Contributors

%.S.o %.o: %.S
	@$(PRINTF) "as $@"
	@$(AS) $(ASFLAGS) $(ASFLAGS.$*.S) $(CPFLAGS) -c -o $@ $(realpath $<)

%.c.o %.o: %.c
	@$(PRINTF) "cc $@"
	@$(CC) $(CCFLAGS) $(CCFLAGS.$*.c) $(CPFLAGS) -c -o $@ $(realpath $<)

%.ld: %.ld.in
	@$(PRINTF) "cpp $@"
	@$(CC) $(CCFLAGS) $(CPFLAGS) -D__ASSEMBLER__=1 -E -xc $(realpath $<) | $(GREP) -v "^#" > $@ || $(TRUE)
