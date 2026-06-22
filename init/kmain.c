#include <limine.h>
#include <vex/binfmt.h>
#include <vex/blk_device.h>
#include <vex/fcntl.h>
#include <vex/file.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/mount.h>
#include <vex/panic.h>
#include <vex/pmap.h>
#include <vex/process.h>
#include <vex/sched.h>
#include <vex/version.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

void __noreturn kmain(void)
{
    pr_emerg("%s %s %s", k_sysname, k_release, k_version);

    invoke_initcalls(initcalls_bsp_start, initcalls_bsp_end);

    const struct blk_device* root_dev = find_blk_device("ada0n1p1");

    if(root_dev == nullptr) {
        panic("root device not found");
    }

    int retval = mount("fat", "/", root_dev, nullptr);

    if(retval < 0) {
        panic("failed to mount root filesystem");
    }

    struct file* procfile = file_open("/utest.elf", O_RDONLY);

    if(procfile == nullptr) {
        panic("failed to open process executable");
    }

    struct process* proc = process_create(nullptr);

    if(proc == nullptr) {
        panic("failed to create process");
    }

    const char* argv[] = { "hello", nullptr };
    const char* envp[] = { "FOO=BAR", nullptr };

    struct entry_point entry;
    retval = load_binary(proc, procfile, argv, envp, &entry);

    if(retval < 0) {
        panic("failed to load process executable");
    }

    struct thread* uthread = create_thread_user(proc, entry.address, entry.ustack);

    if(uthread == nullptr) {
        panic("failed to create process thread");
    }

    sched_enqueue(uthread);

    pr_emerg("handing around...");

    enable_interrupts();

    for(;;) {
        hart_spin();
    }

    unreachable();
}
