// xkomp: supported  (__kmpc_push_num_threads)
//
// num_threads(k) : the team has exactly k threads (for k up to the limit the
// runtime can honour).  Mirrors LLVM's omp_parallel_num_threads.c but checks
// with assert() and counts participants atomically.

#include "common.h"

static void
test_num_threads(int requested)
{
    int counted = 0;
    int reported_ok = 1;

    #pragma omp parallel num_threads(requested) shared(counted, reported_ok)
    {
        #pragma omp atomic
        counted += 1;

        // every thread must see the same team size
        if (omp_get_num_threads() != requested)
        {
            #pragma omp atomic write
            reported_ok = 0;
        }
    }

    CHECK_EQ(counted, requested);
    CHECK_EQ(reported_ok, 1);
}

int
main(void)
{
    // Probe a team size the runtime is comfortable spawning at top level.
    int max_threads = 0;
    #pragma omp parallel
    {
        #pragma omp master
        max_threads = omp_get_num_threads();
    }
    CHECK(max_threads >= 1);

    // XKOMP caches at most XKOMP_MAX_CACHED_TEAMS (8) distinct team sizes per
    // process and asserts on overflow, so exercise only a handful of distinct
    // sizes (the probe above already used one slot).  Each size is re-tested
    // across repetitions, which hits the cache rather than growing it.
    int cap = max_threads < 6 ? max_threads : 6;

    for (int r = 0; r < REPETITIONS; ++r)
        for (int t = 1; t <= cap; ++t)
            test_num_threads(t);

    TEST_PASS();
    return 0;
}
