## SPDX-License-Identifier: BSD-2-Clause
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
.PHONY: boot sys tools

all: boot

clean:
	@$(MAKE) -s -C boot clean
	@$(MAKE) -s -C sys clean
	@$(MAKE) -s -C tools/limine-deploy clean

frc:

boot: frc tools sys
	@$(MAKE) -s -C boot all

sys: frc tools
	@$(MAKE) -s -C sys all

tools: frc
	@$(MAKE) -s -C tools/limine-deploy all
