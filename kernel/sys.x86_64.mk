## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c), 2022, Kaneru Contributors

XXFLAGS	:=
XXFLAGS += -fno-pic
XXFLAGS += -mcmodel=kernel
XXFLAGS += -mno-3dnow -mno-80387 -mno-mmx
XXFLAGS += -mno-red-zone -mno-sse -mno-sse2

ASFLAGS += $(XXFLAGS)
CCFLAGS += $(XXFLAGS)

CPFLAGS += -D __X86_64__
CPFLAGS += -D __x86_64__
CPFLAGS += -D __AMD64__
CPFLAGS += -D __amd64__

LDFLAGS += --no-pie
LDFLAGS += -z max-page-size=0x1000
