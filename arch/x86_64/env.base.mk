override CFLAGS += -fno-stack-protector
override CFLAGS += -fno-stack-check -fno-lto
override CFLAGS += -mno-3dnow -mno-80387 -mno-mmx
override CFLAGS += -mno-red-zone -mno-sse -mno-sse2
override CFLAGS += -m64 -march=x86-64

override CPPFLAGS += -D __AMD64__
override CPPFLAGS += -D __amd64__
override CPPFLAGS += -D __x86_64__
override CPPFLAGS += -D __X86_64__

override LDFLAGS += -m elf_x86_64
override LDFLAGS += -z max-page-size=0x1000
override LDFLAGS += -z text

EFI_ARCH := X64
