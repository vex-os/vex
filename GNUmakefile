## SPDX-License-Identifier: GPL-2.0
## Copyright (c), 2022, Kaneru Contributors

TARGET ?= x86_64
VENDOR ?= kaneru-elf
export TARGET
export VENDOR

VERSION := 0.0.0-dev.1
GIT_REV := $(shell git rev-parse --short=8 HEAD 2> /dev/null || echo 0)
export VERSION
export GIT_REV

.PHONY: all clean frc
.PHONY: boot kern libk tools

all: kern

clean:
	@$(MAKE) -s -C boot clean
	@$(MAKE) -s -C kern clean
	@$(MAKE) -s -C libk clean
	@$(MAKE) -s -C tools/limine-deploy clean

frc:

boot: frc tools kern
	@$(MAKE) -s -C boot all

kern: frc tools libk
	@$(MAKE) -s -C kern all

libk: frc tools
	@$(MAKE) -s -C libk all

tools: frc
	@$(MAKE) -s -C tools/limine-deploy all
