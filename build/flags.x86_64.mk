## SPDX-License-Identifier: GPL-2.0
## Copyright (c), 2022, Kaneru Contributors

XXFLAGS	:=
XXFLAGS += -fno-pic
XXFLAGS += -mcmodel=kernel
XXFLAGS += -mno-3dnow -mno-80387 -mno-mmx
XXFLAGS += -mno-red-zone -mno-sse -mno-sse2

ASFLAGS += $(XXFLAGS)
CCFLAGS += $(XXFLAGS)

CPFLAGS += -D__X86_64__=1 -D__AMD64__=1
CPFLAGS += -D__x86_64__=1 -D__amd64__=1

LDFLAGS += -fno-pie -Wl,-static,--no-dynamic-linker,-ztext
LDFLAGS += -z max-page-size=0x1000

