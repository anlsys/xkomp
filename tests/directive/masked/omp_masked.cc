// xkomp: supported  (`masked` construct: __kmpc_masked / __kmpc_end_masked)
//
// `masked` (OpenMP 5.1) is `master` generalized with a `filter` clause: only the
// thread whose team-local id equals the filter (default 0, the primary thread)
// runs the region. Two sub-tests:
//   1. default filter  -> exactly the primary thread (tid 0) enters.
//   2. filter(k)       -> exactly thread k enters.

#include "common.h"

// (1) Default masked: only the primary thread enters.
static void
test_masked_default(void)
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
}

// (2) masked filter(k): only thread k enters.
static void
test_masked_filter(void)
{
    const int k = 2;
    int entered = 0;
    int who = -1;

    #pragma omp parallel num_threads(4) shared(entered, who) firstprivate(k)
    {
        #pragma omp masked filter(k)
        {
            #pragma omp atomic
            entered += 1;
            who = omp_get_thread_num();
        }

        #pragma omp barrier
    }

    // exactly one thread (tid k) executed the region
    CHECK_EQ(entered, 1);
    CHECK_EQ(who, k);
}

int
main(void)
{
    test_masked_default();
    test_masked_filter();

    TEST_PASS();
    return 0;
}
