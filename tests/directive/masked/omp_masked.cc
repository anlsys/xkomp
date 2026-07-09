// xkomp: XFAIL - `masked` needs __kmpc_masked / __kmpc_end_masked, which XKOMP
//        does not export.  Expected to fail to link.
//        (masked is OpenMP 5.1; an older clang may instead ignore the pragma,
//         in which case `entered` would exceed 1 and the run would fail.)

#include "common.h"

int
main(void)
{
    int entered = 0;
    int who = -1;

    #pragma omp parallel num_threads(4) shared(entered, who)
    {
        #pragma omp masked
        {
            #pragma omp atomic
            entered += 1;
            who = omp_get_thread_num();
        }

        #pragma omp barrier
    }

    CHECK_EQ(entered, 1);
    CHECK_EQ(who, 0);

    TEST_PASS();
    return 0;
}
