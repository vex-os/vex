# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c), 2022, KanOS Contributors
CC := clang
LD := ld.lld
CFLAGS += --target=$(CLANG_TARGET)
CPPFLAGS += -nostdlibinc
