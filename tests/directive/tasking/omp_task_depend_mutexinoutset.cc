// xkomp: supported  (depend(mutexinoutset:) -> commutative/mutually-exclusive)
//
// Many tasks update the same scalar through a mutexinoutset dependence.  Mutual
// exclusion means the updates can happen in any order but never concurrently,
// so a plain (non-atomic) read-modify-write must not lose updates.

#include "common.h"

static void
test_mutexinoutset(void)
{
    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            int x = 0;   // dependency token AND the accumulator

            for (int i = 0; i < NUM_TASKS; ++i)
            {
                #pragma omp task depend(mutexinoutset: x) shared(x)
                {
                    // no atomic on purpose: mutexinoutset must serialize these
                    x = x + 1;
                }
            }

            #pragma omp taskwait
            CHECK_EQ(x, NUM_TASKS);
        }
    }
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_mutexinoutset();

    TEST_PASS();
    return 0;
}
