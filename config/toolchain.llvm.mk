## SPDX-License-Identifier: BSD-2-Clause

CC := clang --target=${LLVM_TARGET}
LD := ld.lld
CPPFLAGS += -nostdlibinc
