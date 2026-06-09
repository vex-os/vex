override CC := clang
override LD := ld.lld
override AR := llvm-ar

override CFLAGS += --target=x86_64-none-elf
override CPPFLAGS += -nostdlibinc

include conf/envt.amd64.base.mk
