// xkomp: supported  (every thread is a task generator)
//
// omp_task.cc creates all tasks from a single producer thread.  Here every
// thread of the team generates its own tasks and joins them with taskwait,
// exercising concurrent task creation from multiple threads.

#include "common.h"

#define PER 16

static void
test_parallel_tasks(void)
{
    static int done[XKOMP_TEST_MAXT * PER];
    memset(done, 0, sizeof(done));
    int nth = 0;

    #pragma omp parallel num_threads(4) shared(done, nth)
    {
        const int tid = omp_get_thread_num();
        if (tid == 0)
            nth = omp_get_num_threads();
        CHECK(tid < XKOMP_TEST_MAXT);

        for (int k = 0; k < PER; ++k)
        {
            #pragma omp task firstprivate(tid, k) shared(done)
            done[tid * PER + k] = 1;
        }
        #pragma omp taskwait   // each thread joins its own children
    }

    for (int t = 0; t < nth; ++t)
        for (int k = 0; k < PER; ++k)
            CHECK_EQ(done[t * PER + k], 1);
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_parallel_tasks();

    TEST_PASS();
    return 0;
}
