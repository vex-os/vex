## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c) 2023, KanOS Contributors

GCC_PREFIX := x86_64-elf
LLVM_TARGET := x86_64-none-elf

CFLAGS += -fno-pic
CFLAGS += -mcmodel=kernel
CFLAGS += -mno-3dnow -mno-80387 -mno-mmx
CFLAGS += -mno-red-zone -mno-sse -mno-sse2

CPPFLAGS += -D __AMD64__
CPPFLAGS += -D __amd64__
CPPFLAGS += -D __X86_64__
CPPFLAGS += -D __x86_64__

LDFLAGS += --no-pie
LDFLAGS += -z max-page-size=0x1000

ISR_STUBS := $(TEMP_DIR)/x86_isr_stubs.S
SOURCES += $(ISR_STUBS)
$(ISR_STUBS): $(TEMP_DIR)
	$(SHELL) scripts/gen_x86_isr_stubs.sh > $@
