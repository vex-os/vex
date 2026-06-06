#include <arch/hart.h>
#include <arch/interrupts.h>
#include <core/assert.h>
#include <core/cdefs.h>
#include <core/cmdline.h>
#include <core/kprintf.h>
#include <core/version.h>
#include <dev/blk_device.h>
#include <fs/vfs_dirent.h>
#include <fs/vfs_mount.h>
#include <fs/vfs_path.h>
#include <fs/vfs_vnode.h>
#include <init/initcall.h>
#include <lib/sprintf.h>
#include <lib/string.h>
#include <limine.h>

// ---------------------------------------------------------------------------
// FAT conformance tests
// ---------------------------------------------------------------------------

static int fat_tests_passed;
static int fat_tests_total;

static void fat_check(bool ok, const char* name)
{
    fat_tests_total++;

    if(ok) {
        fat_tests_passed++;
        pr_verbose("FAT [PASS] %s", name);
    }
    else {
        pr_inform("FAT [FAIL] %s", name);
    }
}

// 32KB: enough to span many clusters even at the smallest FAT16 cluster size (512B)
#define FAT_TEST_BUF 32768
static uint8_t fat_wbuf[FAT_TEST_BUF];
static uint8_t fat_rbuf[FAT_TEST_BUF];

// T1: create a short-name file, write known data, read it back
static void fat_t1_short_write_read(struct vnode* restrict testdir)
{
    const char content[] = "Hello, World!\n";
    const size_t len = sizeof(content) - 1;

    struct vnode* file = nullptr;
    int r = vnode_create(testdir, "test83.txt", VNODE_FILE, &file);
    fat_check(r == 0 && file != nullptr, "T1 create test83.txt");
    if(r != 0 || file == nullptr)
        return;

    ssize_t nw = vnode_write(file, content, len, 0);
    fat_check(nw == (ssize_t)len, "T1 write");

    memset(fat_rbuf, 0, len + 1);
    ssize_t nr = vnode_read(file, fat_rbuf, len, 0);
    fat_check(nr == (ssize_t)len, "T1 read length");
    fat_check(memcmp(fat_rbuf, content, len) == 0, "T1 read content");

    struct __k_stat st;
    vnode_stat(file, &st);
    fat_check((size_t)st.st_size == len, "T1 stat size");

    vnode_decref(file);
}

// T2: long filename (LFN), close then reopen via lookup
static void fat_t2_lfn(struct vnode* restrict testdir)
{
    const char fname[] = "this_is_a_long_filename_test.txt";
    const char content[] = "LFN test data\n";
    const size_t len = sizeof(content) - 1;

    struct vnode* file = nullptr;
    int r = vnode_create(testdir, fname, VNODE_FILE, &file);
    fat_check(r == 0 && file != nullptr, "T2 create LFN file");
    if(r != 0 || file == nullptr)
        return;

    vnode_write(file, content, len, 0);
    vnode_decref(file);

    // Re-open via lookup: exercises fat_dirent_read LFN reassembly
    struct vnode* found = nullptr;
    r = vnode_lookup(testdir, fname, &found);
    fat_check(r == 0 && found != nullptr, "T2 lookup LFN file");
    if(found == nullptr)
        return;

    memset(fat_rbuf, 0, len + 1);
    ssize_t nr = vnode_read(found, fat_rbuf, len, 0);
    fat_check(nr == (ssize_t)len && memcmp(fat_rbuf, content, len) == 0, "T2 read back LFN content");

    vnode_decref(found);
}

// T3: multi-cluster file — write 32KB with a known pattern, read back
static void fat_t3_multicluster(struct vnode* restrict testdir)
{
    for(int i = 0; i < FAT_TEST_BUF; i++) {
        fat_wbuf[i] = i & 0xFF;
    }

    struct vnode* file = nullptr;
    int r = vnode_create(testdir, "multicluster.bin", VNODE_FILE, &file);
    fat_check(r == 0 && file != nullptr, "T3 create multicluster.bin");
    if(r != 0 || file == nullptr)
        return;

    ssize_t nw = vnode_write(file, fat_wbuf, FAT_TEST_BUF, 0);
    fat_check(nw == FAT_TEST_BUF, "T3 write 32KB");

    memset(fat_rbuf, 0, FAT_TEST_BUF);
    ssize_t nr = vnode_read(file, fat_rbuf, FAT_TEST_BUF, 0);
    fat_check(nr == FAT_TEST_BUF, "T3 read back 32KB");
    fat_check(memcmp(fat_rbuf, fat_wbuf, FAT_TEST_BUF) == 0, "T3 data matches");

    struct __k_stat st;
    vnode_stat(file, &st);
    fat_check((int)st.st_size == FAT_TEST_BUF, "T3 stat size 32KB");

    vnode_decref(file);
}

// T4: 8.3 name collision — three LFN files whose stems would collide
//     → ~1 ~2 ~3 must be generated; all three must remain independently findable
static void fat_t4_83_collision(struct vnode* restrict testdir)
{
    static const char* const names[] = {
        "verylongname_alpha.txt",
        "verylongname_beta.txt",
        "verylongname_gamma.txt",
    };

    for(int i = 0; i < 3; i++) {
        struct vnode* f = nullptr;
        int r = vnode_create(testdir, names[i], VNODE_FILE, &f);

        char label[64];
        snprintf(label, sizeof(label), "T4 create %s", names[i]);
        fat_check(r == 0 && f != nullptr, label);

        if(f)
            vnode_decref(f);
    }

    for(int i = 0; i < 3; i++) {
        struct vnode* found = nullptr;
        vnode_lookup(testdir, names[i], &found);

        char label[64];
        snprintf(label, sizeof(label), "T4 lookup %s", names[i]);
        fat_check(found != nullptr, label);

        if(found)
            vnode_decref(found);
    }
}

// T5: new directory must contain . and ..
static void fat_t5_dotdot(struct vnode* restrict testdir)
{
    struct vnode* subdir = nullptr;
    int r = vnode_create(testdir, "dotdottest", VNODE_DIRECTORY, &subdir);
    fat_check(r == 0 && subdir != nullptr, "T5 create dotdottest");
    if(r != 0 || subdir == nullptr)
        return;

    struct dirent de;
    bool found_dot = false;
    bool found_dotdot = false;

    for(size_t i = 0; vnode_iterate(subdir, &de, i) == 0; i++) {
        if(strcmp(de.name, ".") == 0)
            found_dot = true;
        if(strcmp(de.name, "..") == 0)
            found_dotdot = true;
    }

    fat_check(found_dot, "T5 . entry present");
    fat_check(found_dotdot, "T5 .. entry present");

    vnode_decref(subdir);
}

// T6: force fat_dirent_find_free to extend the directory cluster chain
//     by creating enough entries to overflow the initial directory cluster
static void fat_t6_dir_extend(struct vnode* restrict testdir)
{
    struct vnode* subdir = nullptr;
    int r = vnode_create(testdir, "dirextend", VNODE_DIRECTORY, &subdir);
    fat_check(r == 0 && subdir != nullptr, "T6 create dirextend");
    if(r != 0 || subdir == nullptr)
        return;

    // 50 files × (1 LFN + 1 SFN) = 100 slots + 2 for . and ..
    // At 512B/cluster that's 7 clusters — well beyond the initial one
    int created = 0;
    char fname[24];

    for(int i = 0; i < 50; i++) {
        snprintf(fname, sizeof(fname), "file%02d.txt", i);
        struct vnode* f = nullptr;

        if(vnode_create(subdir, fname, VNODE_FILE, &f) == 0 && f != nullptr) {
            created++;
            vnode_decref(f);
        }
    }

    fat_check(created == 50, "T6 created 50 files");

    // Count back via iterate (skip . and ..)
    struct dirent de;
    int found = 0;
    for(size_t i = 0; vnode_iterate(subdir, &de, i) == 0; i++) {
        if(de.name[0] != '.')
            found++;
    }

    fat_check(found == 50, "T6 iterate returns 50 files");

    vnode_decref(subdir);
}

// T7: max-length filename (255 chars) — exercises the strncasecmp PATH_NAME_MAX fix.
// Before the fix, lookup used PATH_NAME_SIZE (256) which could read one byte past the
// caller's string when the name was exactly PATH_NAME_MAX long.
static void fat_t7_maxlen_name(struct vnode* restrict testdir)
{
    char fname[PATH_NAME_SIZE];
    memset(fname, 'a', PATH_NAME_MAX);
    fname[PATH_NAME_MAX] = '\0';

    struct vnode* file = nullptr;
    int r = vnode_create(testdir, fname, VNODE_FILE, &file);
    fat_check(r == 0 && file != nullptr, "T7 create 255-char filename");
    if(r != 0 || file == nullptr)
        return;

    const char content[] = "maxlen";
    vnode_write(file, content, sizeof(content) - 1, 0);
    vnode_decref(file);

    struct vnode* found = nullptr;
    r = vnode_lookup(testdir, fname, &found);
    fat_check(r == 0 && found != nullptr, "T7 lookup 255-char filename");

    if(found) {
        memset(fat_rbuf, 0, sizeof(content));
        ssize_t nr = vnode_read(found, fat_rbuf, sizeof(content) - 1, 0);
        fat_check(nr == (ssize_t)(sizeof(content) - 1) && memcmp(fat_rbuf, content, nr) == 0, "T7 read back content");
        vnode_decref(found);
    }
}

// T8: sequential cluster chain extension — write data in multiple passes, each extending
// the cluster chain, then read back. Exercises fat_cluster_link (fix #1: upper nibble
// preservation) and the file_size update path that reads the dir entry (fix #2).
static void fat_t8_chain_extend(struct vnode* restrict testdir)
{
    struct vnode* file = nullptr;
    int r = vnode_create(testdir, "chainext.bin", VNODE_FILE, &file);
    fat_check(r == 0 && file != nullptr, "T8 create chainext.bin");
    if(r != 0 || file == nullptr)
        return;

    // Write in 4 KB chunks to force cluster allocation at each step
    // (cluster size on test image is typically 4 KB or 512 B — 4 KB covers both)
    const size_t chunk = 4096;
    const int passes = 8;
    size_t off = 0;
    bool write_ok = true;

    for(int p = 0; p < passes; p++) {
        memset(fat_wbuf, 'A' + p, chunk);
        ssize_t nw = vnode_write(file, fat_wbuf, chunk, off);
        if(nw != (ssize_t)chunk) {
            write_ok = false;
            break;
        }
        off += chunk;
    }

    fat_check(write_ok, "T8 wrote 8×4KB in chain");

    struct __k_stat st;
    vnode_stat(file, &st);
    fat_check((size_t)st.st_size == (size_t)(chunk * passes), "T8 stat size after chain writes");

    // Read back each chunk and verify the fill byte
    bool read_ok = true;
    off = 0;

    for(int p = 0; p < passes; p++) {
        memset(fat_rbuf, 0, chunk);
        ssize_t nr = vnode_read(file, fat_rbuf, chunk, off);
        if(nr != (ssize_t)chunk) {
            read_ok = false;
            break;
        }

        for(size_t i = 0; i < chunk; i++) {
            if(fat_rbuf[i] != (uint8_t)('A' + p)) {
                read_ok = false;
                break;
            }
        }

        if(!read_ok)
            break;
        off += chunk;
    }

    fat_check(read_ok, "T8 read back each 4KB chunk correctly");
    vnode_decref(file);
}

// T9: first-write cluster allocation — a newly created file has first_cluster==0 in its
// dir entry. The first vnode_write must allocate a cluster and update the dir entry.
// Re-opening the file via lookup must see the cluster and file_size correctly.
// This exercises fix #2 (fat_dirent_read return value in fat_vnode_write).
static void fat_t9_first_write_cluster(struct vnode* restrict testdir)
{
    const char* fname = "firstwrite.txt";
    const char content[] = "first cluster allocation test\n";
    const size_t len = sizeof(content) - 1;

    struct vnode* file = nullptr;
    int r = vnode_create(testdir, fname, VNODE_FILE, &file);
    fat_check(r == 0 && file != nullptr, "T9 create firstwrite.txt");
    if(r != 0 || file == nullptr)
        return;

    // The vnode was just created: first_cluster == 0, size == 0.
    // Write triggers cluster alloc + dir entry update.
    ssize_t nw = vnode_write(file, content, len, 0);
    fat_check(nw == (ssize_t)len, "T9 first write");
    vnode_decref(file);

    // Re-open via lookup: the dir entry on disk must now carry the correct cluster.
    struct vnode* found = nullptr;
    r = vnode_lookup(testdir, fname, &found);
    fat_check(r == 0 && found != nullptr, "T9 lookup after first write");
    if(found == nullptr)
        return;

    struct __k_stat st;
    vnode_stat(found, &st);
    fat_check((size_t)st.st_size == len, "T9 stat size after reopen");

    memset(fat_rbuf, 0, len + 1);
    ssize_t nr = vnode_read(found, fat_rbuf, len, 0);
    fat_check(nr == (ssize_t)len && memcmp(fat_rbuf, content, len) == 0, "T9 read content after reopen");

    vnode_decref(found);
}

static void fat_run_tests(struct vnode* restrict root)
{
    fat_tests_passed = 0;
    fat_tests_total = 0;

    // All tests live under /fattest to keep the root clean
    struct vnode* testdir = nullptr;
    int r = vnode_create(root, "fattest", VNODE_DIRECTORY, &testdir);

    if(r != 0 || testdir == nullptr) {
        pr_warning("FAT TESTS: could not create /fattest (%d), skipping", r);
        return;
    }

    fat_t1_short_write_read(testdir);
    fat_t2_lfn(testdir);
    fat_t3_multicluster(testdir);
    fat_t4_83_collision(testdir);
    fat_t5_dotdot(testdir);
    fat_t6_dir_extend(testdir);
    fat_t7_maxlen_name(testdir);
    fat_t8_chain_extend(testdir);
    fat_t9_first_write_cluster(testdir);

    vnode_decref(testdir);

    pr_inform("FAT TESTS: %d / %d passed", fat_tests_passed, fat_tests_total);
}

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

#include <fs/devfs.h>

static void recurse_dir(struct vnode* restrict node, const char* restrict path)
{
    struct dirent entry;
    struct __k_stat stat;
    size_t index = 0;

    while(0 == vnode_iterate(node, &entry, index++)) {
        if(0 == strcmp(entry.name, ".") || 0 == strcmp(entry.name, "..")) {
            continue;
        }

        const char* codename;

        switch(entry.nodetype) {
            case VNODE_FILE:
                codename = "FIL";
                break;

            case VNODE_DIRECTORY:
                codename = "DIR";
                break;

            case VNODE_BLOCK_DEV:
                codename = "BLK";
                break;

            case VNODE_CHAR_DEV:
                codename = "CHR";
                break;

            case VNODE_SYM_LINK:
                codename = "LNK";
                break;

            default:
                codename = "---";
                break;
        }

        memset(&stat, 0, sizeof(struct __k_stat));
        struct vnode* entry_node;

        if(0 == path_resolve_at(node, entry.name, &entry_node)) {
            vnode_stat(entry_node, &stat);
            vnode_decref(entry_node);
        }

        pr_inform("  %016b %04X [%s] %s/%s", stat.st_mode, stat.st_mode, codename, path, entry.name);

        if(entry.nodetype == VNODE_DIRECTORY) {
            struct vnode* child;

            if(0 == path_resolve_at(node, entry.name, &child)) {
                char subpath[PATH_NAME_SIZE];
                snprintf(subpath, sizeof(subpath), "%s%c%s", path, PATH_SEPARATOR, entry.name);
                recurse_dir(child, subpath);
                vnode_decref(child);
            }
        }
    }
}

static void recurse_root(void)
{
    struct vnode* root = nullptr;
    if(0 == path_resolve("/", &root)) {
        recurse_dir(root, "");
        vnode_decref(root);
    }
    else {
        pr_warning("no root");
    }
}

void __noreturn kmain(void)
{
    pr_emerg("%s %s %s", k_sysname, k_release, k_version);

    invoke_initcalls(initcalls_bsp_start, initcalls_bsp_end);

    enable_interrupts();

    vfs_mount("fat", "/", blk_device_find("ada0n1p1"), nullptr);

    struct vnode* root_node = nullptr;
    struct vnode* dev1_node = nullptr;
    struct vnode* dev2_node = nullptr;

    if(0 == vfs_get_root(&root_node)) {
        vnode_create(root_node, "dev", VNODE_DIRECTORY, &dev1_node);
        vnode_create(root_node, "dev2", VNODE_DIRECTORY, &dev2_node);
        vnode_decref(root_node);

        if(dev1_node != nullptr) {
            vnode_decref(dev1_node);
        }

        if(dev2_node != nullptr) {
            vnode_decref(dev2_node);
        }
    }

    vfs_sync("/");

    vfs_mount("devfs", "/dev", nullptr, nullptr);
    vfs_mount("devfs", "/dev2", nullptr, nullptr);

    recurse_root();

    struct vnode* usr_node = nullptr;
    struct vnode* local_node = nullptr;
    struct vnode* bin_node = nullptr;

    if(0 == vfs_get_root(&root_node)) {
        if(0 == vnode_create(root_node, "usr", VNODE_DIRECTORY, &usr_node)) {
            if(0 == vnode_create(usr_node, "local", VNODE_DIRECTORY, &local_node)) {
                vnode_create(local_node, "bin", VNODE_DIRECTORY, &bin_node);
                if(bin_node)
                    vnode_decref(bin_node);
                vnode_decref(local_node);
            }
            vnode_decref(usr_node);
        }
        vnode_decref(root_node);
    }

    vfs_sync("/");

    if(0 == vfs_get_root(&root_node)) {
        fat_run_tests(root_node);
        vnode_decref(root_node);
    }

    vfs_sync("/");

    struct vnode* root = nullptr;
    struct vnode* a = nullptr;
    struct vnode* b = nullptr;

    vfs_get_root(&root);

    if(0 == path_resolve("/dev/..", &a)) {
        pr_inform("dotdot: /dev/.. %s /", a == root ? "==" : "!=");
        vnode_decref(a);
    }
    else {
        pr_warning("dotdot: /dev/.. failed");
    }

    if(0 == path_resolve("/dev/../..", &b)) {
        pr_inform("dotdot: /dev/../.. %s /", b == root ? "==" : "!=");
        vnode_decref(b);
    }
    else {
        pr_warning("dotdot: /dev/../.. failed");
    }

    // /usr/local/bin/../.. == /usr
    struct vnode* usr = nullptr;
    struct vnode* c = nullptr;

    if(0 == path_resolve("/usr", &usr)) {
        if(0 == path_resolve("/usr/local/bin/../..", &c)) {
            pr_inform("dotdot: /usr/local/bin/../.. %s /usr", c == usr ? "==" : "!=");
            vnode_decref(c);
        }
        else {
            pr_warning("dotdot: /usr/local/bin/../.. failed");
        }
        vnode_decref(usr);
    }

    // /usr/local/../local/bin/.. == /usr/local
    struct vnode* local = nullptr;
    struct vnode* d = nullptr;

    if(0 == path_resolve("/usr/local", &local)) {
        if(0 == path_resolve("/usr/local/../local/bin/..", &d)) {
            pr_inform("dotdot: /usr/local/../local/bin/.. %s /usr/local", d == local ? "==" : "!=");
            vnode_decref(d);
        }
        else {
            pr_warning("dotdot: /usr/local/../local/bin/.. failed");
        }
        vnode_decref(local);
    }

    // /dev/../usr/local/bin/../../../dev/.. == /
    struct vnode* e = nullptr;

    if(0 == path_resolve("/dev/../usr/local/bin/../../../dev/..", &e)) {
        pr_inform("dotdot: long path %s /", e == root ? "==" : "!=");
        vnode_decref(e);
    }
    else {
        pr_warning("dotdot: long path failed");
    }

    vnode_decref(root);

    pr_emerg("hanging around...");

    for(;;) {
        stop_hart();
    }

    unreachable();
}
