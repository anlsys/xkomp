// xkomp: supported  (__kmpc_barrier)
//
// After a barrier every thread must observe the writes performed by all other
// threads before the barrier.  Thread 0 deliberately writes late (after a short
// sleep) so a broken/no-op barrier would let the other threads read the stale
// value.

#include "common.h"
#include <unistd.h>

static void
test_barrier(void)
{
    int result = 0;
    static int flags[XKOMP_TEST_MAXT];
    memset(flags, 0, sizeof(flags));

    #pragma omp parallel num_threads(4) shared(result, flags)
    {
        const int tid = omp_get_thread_num();
        const int nth = omp_get_num_threads();

        if (tid == 0)
        {
            usleep(20 * 1000);   // 20 ms: make thread 0 the slow producer
            result = 100;
        }
        flags[tid] = tid + 1;

        #pragma omp barrier

        // every thread must now see thread 0's late write and all flags
        CHECK_EQ(result, 100);
        for (int i = 0; i < nth; ++i)
            CHECK_EQ(flags[i], i + 1);
    }
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_barrier();

    TEST_PASS();
    return 0;
}
