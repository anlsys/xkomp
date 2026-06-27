// xkomp: supported  (__kmpc_omp_taskwait)
//
// taskwait must block until the child tasks generated so far have completed.
// A child writes a value after a short sleep; without taskwait the parent would
// race, with taskwait it must observe the write.

#include "common.h"
#include <unistd.h>

static void
test_taskwait(void)
{
    int a = 0;
    int b = 0;

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            #pragma omp task shared(a)
            {
                usleep(10 * 1000);
                a = 11;
            }
            #pragma omp task shared(b)
            {
                usleep(10 * 1000);
                b = 22;
            }

            #pragma omp taskwait
            CHECK_EQ(a, 11);
            CHECK_EQ(b, 22);
        }
    }
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_taskwait();

    TEST_PASS();
    return 0;
}
