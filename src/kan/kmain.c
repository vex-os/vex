/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/vfs.h>
#include <kan/debug.h>
#include <kan/errno.h>
#include <kan/initcall.h>
#include <kan/interrupt.h>
#include <kan/kmalloc.h>
#include <kan/kprintf.h>
#include <kan/symbol.h>
#include <kan/version.h>
#include <stdlib.h>
#include <string.h>

static int int42_handler(interrupt_frame_t *restrict frame, void *restrict arg)
{
    pr_inform("interrupts test: handler called (int $%#02zX)", (size_t)frame->vector);
    return INTERRUPT_CONSUME;
}

void __noreturn __used kmain(void)
{
    int r;
    size_t i;
    void *p1;
    void *p2;
    void *p3;

    /* Print some information about ourselves... */
    pr_inform("starting version %s", kernel_semver);

    for(i = 0; __initcalls[i].func; i++) {
        r = __initcalls[i].func();

        /* An initcall returning ENODEV is a special case,
         * it just means the initcall wasn't successful but
         * it didn't fail either (for instance there was no
         * device associated with the driver or whatever). */
        if(r != EOK && r != ENODEV) {
            panic("%s: %s", __initcalls[i].name, strerror(r));
            unreachable();
        }
    }

    /* Test interrupt handling */
    add_interrupt_handler(alloc_interrupt(0x42), &int42_handler, NULL);
    add_interrupt_handler(alloc_interrupt(0x43), &int42_handler, NULL);
    add_interrupt_handler(alloc_interrupt(0x44), &int42_handler, NULL);
    asm volatile("int $0x42");
    asm volatile("int $0x43");
    asm volatile("int $0x44");

    /* Test export table */
    pr_inform("kputs: export=%p kernel=%p", get_export("kputs"), (void *)(&kputs));
    pr_inform("kmalloc: export=%p kernel=%p", get_export("kmalloc"), (void *)(&kmalloc));

    /* Test kmalloc */
    p1 = kmalloc(64);
    p2 = kmalloc(64);
    pr_inform("kmalloc test 1: p1=%p p2=%p", p1, p2);
    kfree(p1);
    p3 = kmalloc(64);
    pr_inform("kmalloc test 2: p1=%p p3=%p", p1, p3);
    kfree(p2);
    kfree(p3);
    p1 = kmalloc(8192);
    pr_inform("kmalloc test 3: p1=%p", p1);
    kfree(p1);

    pr_inform("Unicode test: ЕСЛИ ЭТО ЧИТАЕМО, ТО ЮНИКОД ПОДДЕРЖИВАЕТСЯ");
    pr_inform("\x1B[91mэтот текст должен быть красным \x1B[0m");
    pr_inform("\x1B[92mэтот текст должен быть зелёным \x1B[0m");
    pr_inform("\x1B[94mэтот текст должен быть синим \x1B[0m");

    pr_inform("ANSI escape test");
    pr_inform("NORMAL BG:\t\x1B[0m"
        "\x1B[40m    \x1B[41m    "
        "\x1B[42m    \x1B[43m    "
        "\x1B[44m    \x1B[45m    "
        "\x1B[46m    \x1B[47m    \x1B[0m");
    pr_inform("BRIGHT BG:\t\x1B[0m"
        "\x1B[100m    \x1B[101m    "
        "\x1B[102m    \x1B[103m    "
        "\x1B[104m    \x1B[105m    "
        "\x1B[106m    \x1B[107m    \x1B[0m");
    pr_inform("FAINT  FG:\t\x1B[2m"
        "\x1B[30mXXXX\x1B[31mXXXX"
        "\x1B[32mXXXX\x1B[33mXXXX"
        "\x1B[34mXXXX\x1B[35mXXXX"
        "\x1B[36mXXXX\x1B[37mXXXX\x1B[0m");
    pr_inform("NORMAL FG:\t\x1B[0m"
        "\x1B[30mXXXX\x1B[31mXXXX"
        "\x1B[32mXXXX\x1B[33mXXXX"
        "\x1B[34mXXXX\x1B[35mXXXX"
        "\x1B[36mXXXX\x1B[37mXXXX\x1B[0m");
    pr_inform("BRIGHT FG:\t\x1B[1m"
        "\x1B[90mXXXX\x1B[91mXXXX"
        "\x1B[92mXXXX\x1B[93mXXXX"
        "\x1B[94mXXXX\x1B[95mXXXX"
        "\x1B[96mXXXX\x1B[97mXXXX\x1B[0m");
    pr_inform("x\ttab");
    pr_inform("xx\ttab");
    pr_inform("xxx\ttab");
    pr_inform("xxxx\ttab");
    pr_inform("xxxxx\ttab");

    /* Test VFS */
    uint64_t superblock_base = 0x01000000;
    vfs_super_t superblock = vfs_register_fs("test_fs", 1024, (void *)superblock_base);
    pr_inform("vfs: registered filesystem");
    pr_inform("vfs: fs_id=%lu", superblock.fs_id);
    pr_inform("vfs: fs_name=%s",superblock.fs_name);
    pr_inform("vfs: fs_block_size=%zu",superblock.fs_block_size);
    pr_inform("vfs: nblocks=%lu",superblock.nblocks);
    pr_inform("vfs: ninodes=%lu",superblock.ninodes);
    pr_inform("vfs: base=%p",superblock.base);
        
    vfs_mount_fs("/", superblock, false);
    pr_inform("vfs: mounted %s at %s", superblock.fs_name, (char *)superblock_base);

    vfs_create("/", "testdir/");
    vfs_create("/testdir/", "test_file_1"); 
    vfs_create("/", "test_file_2");
    vfs_create("/testdir/", "test_file_3");

    vfs_remove("/testdir/test_file_3", "/");
    vfs_remove("/testdir/test_file_1", "/");
    
    vfs_unmount_fs(superblock);
    pr_inform("vfs: unmounted %s", superblock.fs_name);
    
    vfs_unregister_fs(superblock);
    pr_inform("vfs: unregistered filesystem");
    
    panic("nothing to do");
    unreachable();
}

