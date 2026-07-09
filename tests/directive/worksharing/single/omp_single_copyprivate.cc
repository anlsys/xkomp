// xkomp: XFAIL - `single copyprivate(x)` broadcasts the value via
//        __kmpc_copyprivate, which XKOMP does not export.  Expected link fail.

#include "common.h"

static void
test_copyprivate(void)
{
    #pragma omp parallel num_threads(4)
    {
        int local = 0;

        #pragma omp single copyprivate(local)
        {
            local = 99;
        }

        // copyprivate must broadcast the single thread's value to all threads
        CHECK_EQ(local, 99);
    }
}

int
main(void)
{
    test_copyprivate();
    TEST_PASS();
    return 0;
}
