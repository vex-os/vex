## SPDX-License-Identifier: GPL-2.0
## Copyright (c), 2022, Kaneru Contributors
TOPDIR := ../..
ECHO_PREFIX := tools
include $(TOPDIR)/build/prefix.none.mk
include $(TOPDIR)/build/flags.default.mk
include $(TOPDIR)/build/rules.mk
include $(TOPDIR)/build/tools.mk

PHONY_TARGETS :=
CCFLAGS += -O2 -pipe -Wall -Wextra -std=c11

PHONY_TARGETS += all
all: limine-deploy

PHONY_TARGETS += clean
clean:
	@$(RM) -v limine-deploy

limine-deploy: limine-deploy.c limine-hdd.h
	@$(PRINTF) "building $@"
	@$(CC) $(CCFLAGS) $(CPFLAGS) limine-deploy.c -o $@

# Set up phony targets
.phony: $(PHONY_TARGETS)
