// xkomp: supported  (static schedule via __kmpc_for_static_init_4 / _fini)
//
// A worksharing loop distributes its iteration space so that every iteration is
// executed exactly once across the team.

#include "common.h"

static void
test_for(void)
{
    static int seen[LOOPCOUNT];
    memset(seen, 0, sizeof(seen));

    int total = 0;

    #pragma omp parallel num_threads(4) shared(seen, total)
    {
        #pragma omp for
        for (int i = 0; i < LOOPCOUNT; ++i)
        {
            // distinct i per thread, but use atomics so an erroneous overlap
            // cannot be masked by a lost update
            #pragma omp atomic
            seen[i] += 1;

            #pragma omp atomic
            total += 1;
        }
    }

    CHECK_EQ(total, LOOPCOUNT);
    for (int i = 0; i < LOOPCOUNT; ++i)
        CHECK_EQ(seen[i], 1);
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_for();

    TEST_PASS();
    return 0;
}
