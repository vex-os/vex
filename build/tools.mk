## SPDX-License-Identifier: GPL-2.0
## Copyright (c), 2022, Kaneru Contributors
ECHO_PREFIX	?= build

CD			:= cd
CP			:= cp
PRINTF		:= printf "[$(ECHO_PREFIX)] %s\n"
ENVSUBST	:= envsubst
GIT			:= git
GREP		:= grep
MKDIR		:= mkdir
RM			:= rm -f
RSYNC		:= rsync
TRUE		:= true

AS			:= $(ARCH)$(DASH)$(VEND)$(DASH)gcc
AR			:= $(ARCH)$(DASH)$(VEND)$(DASH)ar
CC			:= $(ARCH)$(DASH)$(VEND)$(DASH)gcc
CPP			:= $(ARCH)$(DASH)$(VEND)$(DASH)cpp
LD			:= $(ARCH)$(DASH)$(VEND)$(DASH)gcc -g
OBJDUMP		:= $(ARCH)$(DASH)$(VEND)$(DASH)objdump
