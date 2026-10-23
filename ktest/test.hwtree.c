#include <vex/errno.h>
#include <vex/fcntl.h>
#include <vex/hwtree.h>
#include <vex/syscall.h>

#include "ktest.h"

/* Records come in whole, aligned to __k_FEED_ALIGN; a read may give several */
static unsigned long long hwtree_buffer[4096 / sizeof(unsigned long long)];
static long hwtree_offset;
static long hwtree_length;

/* Drops what hwtree_next has read ahead, as after a resnapshot */
static void hwtree_forget(void)
{
    hwtree_offset = 0;
    hwtree_length = 0;
}

/* The next record from the file, read when the buffer runs out; null with the read's result in *result */
static const struct __k_hwtree_record* hwtree_next(long fd, long* result)
{
    const struct __k_hwtree_record* record;

    if(hwtree_offset == hwtree_length) {
        hwtree_forget();
        *result = ktest_syscall3(__k_SYS_READ, fd, (long)hwtree_buffer, sizeof(hwtree_buffer));

        if(*result <= 0) {
            return 0;
        }

        hwtree_length = *result;
    }

    record = (const struct __k_hwtree_record*)((const char*)hwtree_buffer + hwtree_offset);
    hwtree_offset += record->feed.size;
    *result = record->feed.size;

    return record;
}

/* The value of the record's field, or null */
static const char* hwtree_field(const struct __k_hwtree_record* record, const char* key)
{
    long length = ktest_strlen(key);
    long offset = 0;

    while(offset < (long)record->fields_size) {
        const char* field = record->fields + offset;

        if(ktest_bytes_equal(field, key, length) && field[length] == '=') {
            return field + length + 1;
        }

        offset += ktest_strlen(field) + 1;
    }

    return 0;
}

int main(void)
{
    const struct __k_hwtree_record* record;
    const char* kind;
    char small[8];
    long reader = ktest_open("/dev/hwtree", __k_O_RDONLY | __k_O_NONBLOCK, 0);
    long writer = ktest_open("/dev/hwtree", __k_O_WRONLY, 0);
    long result;
    __k_uint64_t id = 0;
    __k_uint64_t missing = ~(__k_uint64_t)0;
    int began, ended = 0, parents_first = 1, seen_block = 0;
    __k_uint64_t seen[256];
    long seen_count = 0;
    long i;

    ktest_expect(reader >= 0 && writer >= 0, 1);

    hwtree_forget();
    record = hwtree_next(reader, &result);
    began = record && record->feed.type == __k_FEED_TYPE_HWTREE_SNAPSHOT_BEGIN;

    while(!ended && (record = hwtree_next(reader, &result))) {
        if(record->feed.type == __k_FEED_TYPE_HWTREE_ADD) {
            int known = record->parent == 0;

            for(i = 0; i < seen_count; ++i) {
                known |= seen[i] == record->parent;
            }

            parents_first &= known;

            if(seen_count < 256) {
                seen[seen_count] = record->id;
                seen_count += 1;
            }

            if(id == 0) {
                id = record->id;
            }
        }

        kind = hwtree_field(record, "kind");
        seen_block |= record->feed.type == __k_FEED_TYPE_HWTREE_INTERFACE && kind && ktest_bytes_equal(kind, "block", 6);
        ended = record->feed.type == __k_FEED_TYPE_HWTREE_SNAPSHOT_END;
    }

    ktest_expect(began && ended, 1);
    ktest_expect(seen_count > 0 && parents_first, 1);
    ktest_expect(seen_block, 1);

    hwtree_next(reader, &result);
    ktest_expect(result, -__k_EAGAIN);

    ktest_expect(ktest_syscall3(__k_SYS_IOCTL, reader, __k_HWTREE_RESNAPSHOT, 0), 0);
    ktest_expect(ktest_syscall3(__k_SYS_READ, reader, (long)small, sizeof(small)), -__k_EINVAL);
    hwtree_forget();
    record = hwtree_next(reader, &result);
    ktest_expect(record && record->feed.type == __k_FEED_TYPE_HWTREE_SNAPSHOT_BEGIN, 1);

    ktest_expect(ktest_syscall3(__k_SYS_IOCTL, reader, __k_HWTREE_RESCAN, (long)&id), -__k_EPERM);
    ktest_expect(ktest_syscall3(__k_SYS_IOCTL, writer, __k_HWTREE_RESCAN, (long)&missing), -__k_ENOENT);
    ktest_expect(ktest_syscall3(__k_SYS_IOCTL, writer, __k_HWTREE_RESCAN, (long)&id), 0);
    ktest_expect(ktest_syscall3(__k_SYS_READ, writer, (long)small, sizeof(small)), -__k_EBADF);

    ktest_close(writer);
    ktest_close(reader);

    return ktest_result();
}
