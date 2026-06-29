// xkomp: supported  (__kmpc_master / __kmpc_end_master)
//
// Only the master thread (tid 0) executes the region.  master has NO implicit
// barrier, so we add an explicit one before cross-thread checks.

#include "common.h"

static void
test_master(void)
{
    int entered = 0;
    int who = -1;

    #pragma omp parallel num_threads(4) shared(entered, who)
    {
        #pragma omp master
        {
            #pragma omp atomic
            entered += 1;
            who = omp_get_thread_num();
        }

        #pragma omp barrier   // master has no implicit barrier

        CHECK_EQ(entered, 1);
        CHECK_EQ(who, 0);     // the master is thread 0
    }

    CHECK_EQ(entered, 1);
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_master();

    TEST_PASS();
    return 0;
}
