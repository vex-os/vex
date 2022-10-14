## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c), 2022, Kaneru Contributors

## Cross-compile
ASFLAGS	+= --target=$(ARCH)-none-elf
CCFLAGS	+= --target=$(ARCH)-none-elf
