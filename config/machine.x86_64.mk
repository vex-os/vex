## SPDX-License-Identifier: GPL-2.0-only
## Copyright (c) 2023, VX/sys Contributors

GCC_PREFIX := x86_64-elf
LLVM_TARGET := x86_64-none-elf
LIMINE_ARCH := x86-64
EFI_ARCH := X64

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
