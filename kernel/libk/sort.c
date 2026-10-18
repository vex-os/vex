#include <vex/stdlib.h>

static void swap_bytes(unsigned char* restrict a, unsigned char* restrict b, size_t size)
{
    for(size_t i = 0; i < size; ++i) {
        unsigned char tmp = a[i];
        a[i] = b[i];
        b[i] = tmp;
    }
}

static void sift_down(unsigned char* base, size_t root, size_t count, size_t size, int (*cmp)(const void*, const void*))
{
    while(2 * root + 1 < count) {
        size_t child = 2 * root + 1;

        if(child + 1 < count && cmp(&base[(child + 1) * size], &base[child * size]) > 0) {
            child += 1;
        }

        if(cmp(&base[root * size], &base[child * size]) >= 0) {
            return;
        }

        swap_bytes(&base[root * size], &base[child * size], size);
        root = child;
    }
}

void sort(void* base, size_t count, size_t size, int (*cmp)(const void*, const void*))
{
    unsigned char* bytes = base;

    for(size_t i = count / 2; i > 0; --i) {
        sift_down(bytes, i - 1, count, size, cmp);
    }

    for(size_t end = count; end > 1; end -= 1) {
        swap_bytes(&bytes[0], &bytes[(end - 1) * size], size);
        sift_down(bytes, 0, end - 1, size, cmp);
    }
}
