#include "ktest.h"

int main(int argc, char** argv)
{
    ktest_printf("hello from %s\n", argv[0]);
    ktest_assert(argc == 1);

    return 0;
}
