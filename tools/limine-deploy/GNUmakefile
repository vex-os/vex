## SPDX-License-Identifier: GPL-2.0
## Copyright (c), 2022, Kaneru Contributors

TREE_ROOT := ../..
PRINTF_PREFIX := tools

include $(TREE_ROOT)/scripts/mk/common.mk
include $(TREE_ROOT)/scripts/mk/host_override.mk

CCFLAGS :=
CCFLAGS += -O2 -pipe
CCFLAGS += -pedantic -Wall -Wextra -Werror
CCFLAGS += -std=gnu99

BINARY := limine-deploy

.PHONY: all clean

all: $(BINARY)

clean:
	@$(RM) -v $(BINARY)

$(BINARY): limine-deploy.c limine-hdd.h
	@$(PRINTF) "compiling and linking $@"
	@$(CC) $(CCFLAGS) $(CPFLAGS) -o $@ limine-deploy.c
