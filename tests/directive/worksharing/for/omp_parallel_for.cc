// xkomp: supported  (combined `parallel for`)
//
// The combined construct must both spawn a team and distribute the loop, with
// every iteration executed once.

#include "common.h"

static void
test_parallel_for(void)
{
    static int seen[LOOPCOUNT];
    memset(seen, 0, sizeof(seen));

    #pragma omp parallel for num_threads(4) shared(seen)
    for (int i = 0; i < LOOPCOUNT; ++i)
    {
        #pragma omp atomic
        seen[i] += 1;
    }

    for (int i = 0; i < LOOPCOUNT; ++i)
        CHECK_EQ(seen[i], 1);
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_parallel_for();

    TEST_PASS();
    return 0;
}
