// xkomp: supported  (single nowait: no implicit barrier at the end)
//
// Still exactly one thread executes the region, but threads are not
// synchronized at its end -- so we add an explicit barrier before the
// cross-thread check.

#include "common.h"

static void
test_single_nowait(void)
{
    int entered = 0;

    #pragma omp parallel num_threads(4) shared(entered)
    {
        #pragma omp single nowait
        {
            #pragma omp atomic
            entered += 1;
        }

        // no implicit barrier from `single nowait`; synchronize explicitly
        #pragma omp barrier
        CHECK_EQ(entered, 1);
    }

    CHECK_EQ(entered, 1);
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_single_nowait();

    TEST_PASS();
    return 0;
}
