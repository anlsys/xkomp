// xkomp: XFAIL - reduction on a worksharing loop needs __kmpc_reduce_nowait /
//        __kmpc_end_reduce_nowait, not exported by XKOMP.  Expected link fail.

#include "common.h"

int
main(void)
{
    long sum = 0;

    #pragma omp parallel num_threads(4)
    {
        #pragma omp for reduction(+ : sum)
        for (int i = 0; i < LOOPCOUNT; ++i)
            sum += i;
    }

    CHECK_EQ(sum, (long)(LOOPCOUNT - 1) * LOOPCOUNT / 2);

    TEST_PASS();
    return 0;
}
