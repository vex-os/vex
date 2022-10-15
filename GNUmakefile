## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c), 2022, Kaneru Contributors

ARCH	?= x86_64
VERSION := 0.0.0-dev.1
GIT_REV	:= $(shell git rev-parse --short=8 HEAD 2> /dev/null || echo 0)

export ARCH
export VERSION
export GIT_REV

.PHONY: all clean frc
.PHONY: boot kernel tools

all: boot

clean:
	@$(MAKE) -C boot clean
	@$(MAKE) -C kernel clean
	@$(MAKE) -C tools/limine-deploy clean

frc:

boot: frc tools kernel
	@$(MAKE) -C boot all

kernel: frc tools
	@$(MAKE) -C kernel all

tools: frc
	@$(MAKE) -C tools/limine-deploy all
