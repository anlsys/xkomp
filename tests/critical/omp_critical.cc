// xkomp: XFAIL - `critical` needs __kmpc_critical / __kmpc_end_critical, which
//        XKOMP does not export.  Expected to fail to link.

#include "common.h"

int
main(void)
{
    int counter = 0;
    int nth = 0;

    #pragma omp parallel num_threads(4) shared(counter, nth)
    {
        if (omp_get_thread_num() == 0)
            nth = omp_get_num_threads();

        for (int k = 0; k < LOOPCOUNT; ++k)
        {
            #pragma omp critical
            counter += 1;   // serialized, so no lost updates
        }
    }

    CHECK_EQ(counter, nth * LOOPCOUNT);

    TEST_PASS();
    return 0;
}
