// xkomp: XFAIL - `taskyield` needs __kmpc_omp_taskyield, which XKOMP does not
//        export.  Expected to fail to link.

#include "common.h"

static void
test_taskyield(void)
{
    int done = 0;

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            #pragma omp task shared(done)
            {
                #pragma omp taskyield
                done = 1;
            }
            #pragma omp taskwait
            CHECK_EQ(done, 1);
        }
    }
}

int
main(void)
{
    test_taskyield();
    TEST_PASS();
    return 0;
}
