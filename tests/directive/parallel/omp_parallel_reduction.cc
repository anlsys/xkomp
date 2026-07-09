// xkomp: XFAIL - reduction clause needs __kmpc_reduce_nowait/__kmpc_end_reduce*,
//        which XKOMP does not export.  Expected to fail to link.
//        (Workaround pattern lives in composite/omp_manual_reduction.cc.)

#include "common.h"

int
main(void)
{
    int sum = 0;

    #pragma omp parallel num_threads(4) reduction(+ : sum)
    {
        sum += omp_get_thread_num() + 1;
    }

    // if this ever links & runs: 1+2+...+nth
    CHECK(sum >= 1);

    TEST_PASS();
    return 0;
}
