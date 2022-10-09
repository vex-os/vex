## SPDX-License-Identifier: GPL-2.0
## Copyright (c), 2022, Kaneru Contributors

ARCH ?= x86_64
DASH := -
VEND := $(shell scripts/getvend.sh $(ARCH))
PHONY_TARGETS :=

export ARCH
export DASH
export VEND

PHONY_TARGETS += frc
frc:

PHONY_TARGETS += all
all: kernel boot

PHONY_TARGETS += clean
clean:
	@$(MAKE) -s -C boot clean
	@$(MAKE) -s -C kernel clean
	@$(MAKE) -s -C libkern clean
	@$(MAKE) -s -C tools/limine-deploy clean

PHONY_TARGETS += boot
boot: tools frc
	@$(MAKE) -s -C boot all

PHONY_TARGETS += kernel
kernel: libkern
	@$(MAKE) -s -C kernel all

PHONY_TARGETS += libkern
libkern: frc
	@$(MAKE) -s -C libkern all

PHONY_TARGETS += tools
tools: frc
	@$(MAKE) -s -C tools/limine-deploy all

# Setup phony targets
.phony: $(PHONY_TARGETS)
