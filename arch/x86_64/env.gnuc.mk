override CC := x86_64-elf-gcc
override LD := x86_64-elf-ld
override AR := x86_64-elf-ar

override CPPFLAGS += -nostdlib

include arch/x86_64/env.base.mk
