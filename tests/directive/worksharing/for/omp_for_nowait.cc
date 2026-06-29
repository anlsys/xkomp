// xkomp: supported  (nowait drops the worksharing loop's implicit barrier)
//
// Two consecutive nowait loops over independent data: correctness must hold
// without the barrier between them.

#include "common.h"

static void
test_nowait(void)
{
    static int a[LOOPCOUNT];
    static int b[LOOPCOUNT];
    memset(a, 0, sizeof(a));
    memset(b, 0, sizeof(b));

    #pragma omp parallel num_threads(4) shared(a, b)
    {
        #pragma omp for nowait
        for (int i = 0; i < LOOPCOUNT; ++i)
            a[i] = i + 1;

        #pragma omp for nowait
        for (int i = 0; i < LOOPCOUNT; ++i)
            b[i] = 2 * (i + 1);
    }

    for (int i = 0; i < LOOPCOUNT; ++i)
    {
        CHECK_EQ(a[i], i + 1);
        CHECK_EQ(b[i], 2 * (i + 1));
    }
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_nowait();

    TEST_PASS();
    return 0;
}
