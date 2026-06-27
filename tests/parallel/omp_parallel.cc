// xkomp: supported  (__kmpc_fork_call)
//
// #pragma omp parallel : every thread of the team runs the region exactly once.

#include "common.h"

static void
test_omp_parallel(void)
{
    static int seen[XKOMP_TEST_MAXT];
    memset(seen, 0, sizeof(seen));

    int team_size = 0;

    #pragma omp parallel num_threads(4) shared(seen, team_size)
    {
        const int tid = omp_get_thread_num();
        const int nth = omp_get_num_threads();

        CHECK(nth >= 1);
        CHECK(nth <= XKOMP_TEST_MAXT);
        CHECK(tid >= 0);
        CHECK(tid < nth);

        seen[tid] = 1;

        // all threads observe the same team size; the trailing implicit barrier
        // makes this write visible to the post-region read below
        if (tid == 0)
            team_size = nth;
    }

    CHECK(team_size >= 1);

    int count = 0;
    for (int i = 0; i < XKOMP_TEST_MAXT; ++i)
        count += seen[i];

    // exactly the threads [0, team_size) ran, once each
    CHECK_EQ(count, team_size);
    for (int i = 0; i < team_size; ++i)
        CHECK_EQ(seen[i], 1);
}

int
main(void)
{
    // outside any parallel region the spec mandates 1
    CHECK_EQ(omp_get_num_threads(), 1);

    for (int r = 0; r < REPETITIONS; ++r)
        test_omp_parallel();

    TEST_PASS();
    return 0;
}
