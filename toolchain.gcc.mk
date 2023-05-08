# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c), 2022, KanOS Contributors
# FIXME: this shit may not even work
CC := $(GCC_TARGET)-gcc
LD := $(GCC_TARGET)-ld
CPPFLAGS += -nostdlib
