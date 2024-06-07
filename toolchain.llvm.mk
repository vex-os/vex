## SPDX-License-Identifier: GPL-2.0-only

CC := clang --target=$(LLVM_TARGET)
LD := ld.lld
CPPFLAGS += -nostdlibinc
