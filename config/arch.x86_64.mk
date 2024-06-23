## SPDX-License-Identifier: Zlib

GNU_TARGET_TRIPLET := x86_64-elf
LLVM_TARGET := x86_64-none-elf
LIMINE_ARCH := x86-64
UEFI_ARCH := X64

CFLAGS += -fno-pic
CFLAGS += -fno-stack-protector
CFLAGS += -fno-stack-check -fno-lto
CFLAGS += -mno-3dnow -mno-80387 -mno-mmx
CFLAGS += -mno-red-zone -mno-sse -mno-sse2
CFLAGS += -m64 -march=x86-64
CFLAGS += -mcmodel=kernel

CPPFLAGS += -D __AMD64__
CPPFLAGS += -D __amd64__
CPPFLAGS += -D __X86_64__
CPPFLAGS += -D __x86_64__

LDFLAGS += --no-pie
LDFLAGS += -m elf_x86_64
LDFLAGS += -z max-page-size=0x1000
LDFLAGS += -z text
