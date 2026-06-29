// xkomp: supported  (data-sharing handled by clang codegen)
//
// firstprivate / shared / private data-sharing on a parallel region.

#include "common.h"

static void
test_firstprivate(void)
{
    const int fp_init = 12345;
    int fp = fp_init;          // firstprivate: each thread gets its own copy = fp_init
    int shared_sum = 0;        // shared: combined atomically

    static int saw_init[XKOMP_TEST_MAXT];
    memset(saw_init, 0, sizeof(saw_init));

    int team_size = 0;

    #pragma omp parallel num_threads(4) firstprivate(fp) shared(shared_sum, saw_init, team_size)
    {
        const int tid = omp_get_thread_num();

        // every thread must observe the captured initial value
        saw_init[tid] = (fp == fp_init);

        // mutate the private copy: must not leak to other threads or outside
        fp += tid + 1;

        #pragma omp atomic
        shared_sum += 1;

        if (tid == 0)
            team_size = omp_get_num_threads();
    }

    // the original variable is unchanged after the region (private copies inside)
    CHECK_EQ(fp, fp_init);
    CHECK_EQ(shared_sum, team_size);
    for (int i = 0; i < team_size; ++i)
        CHECK_EQ(saw_init[i], 1);
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_firstprivate();

    TEST_PASS();
    return 0;
}
