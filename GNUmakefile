## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c), 2022, Kaneru Contributors

ARCH	?= x86_64
VERSION := 0.0.0-dev.1
GIT_REV	:= $(shell git rev-parse --short=8 HEAD 2> /dev/null || echo 0)
KCFLAGS	?=

export ARCH
export VERSION
export GIT_REV
export KCFLAGS

.PHONY: all clean frc
.PHONY: boot sys tools

all: boot

clean:
	@$(MAKE) -s -C sys clean
	@$(MAKE) -s -C boot clean
	@$(MAKE) -s -C tools/limine-deploy clean

frc:

boot: frc tools sys
	@$(MAKE) -s -C boot all

sys: frc tools
	@$(MAKE) -s -C sys all

tools: frc
	@$(MAKE) -s -C tools/limine-deploy all
