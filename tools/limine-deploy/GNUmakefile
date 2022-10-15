## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c), 2022, Kaneru Contributors

TOP_DIR		:= ../..
include $(TOP_DIR)/build/tools.base.mk
include $(TOP_DIR)/build/tools.compilers.mk

CCFLAGS		+= -O2 -pipe
CCFLAGS		+= -Wall -Wextra -Werror
CCFLAGS		+= -std=gnu99

TARGET		:= limine-deploy

.PHONY: all clean

all: $(TARGET)

clean:
	$(RM) $(TARGET)

$(TARGET): limine-deploy.c limine-hdd.h
	$(CC) $(CCFLAGS) $(CPFLAGS) -o $@ limine-deploy.c
