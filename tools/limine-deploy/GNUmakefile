## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c), 2022, Kaneru Contributors

TOP_DIR		:= ../..
LOG_PFX		:= tools
include $(TOP_DIR)/build/base.mk

CCFLAGS		+= -O2 -pipe
CCFLAGS		+= -Wall -Wextra -Werror
CCFLAGS		+= -std=gnu99

TARGET		:= limine-deploy

.PHONY: all clean

all: $(TARGET)

clean:
	@$(RMV) $(TARGET)

$(TARGET): limine-deploy.c limine-hdd.h
	@$(LOG) "building $@"
	@$(CLANG) $(CCFLAGS) $(CPFLAGS) -o $@ limine-deploy.c
