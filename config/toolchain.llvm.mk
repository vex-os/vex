## SPDX-License-Identifier: Zlib

CC := clang --target=$(LLVM_TARGET)
LD := ld.lld
CPPFLAGS += -nostdlibinc
