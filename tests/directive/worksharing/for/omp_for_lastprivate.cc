// xkomp: supported  (lastprivate via the plastiter flag; firstprivate codegen)
//
// lastprivate must end up holding the value produced by the *logically last*
// iteration; firstprivate must enter each chunk with the captured value.

#include "common.h"

static void
test_lastprivate(void)
{
    const int base = 5;
    int last = -999;        // lastprivate target

    #pragma omp parallel num_threads(4)
    {
        #pragma omp for firstprivate(base) lastprivate(last)
        for (int i = 0; i < LOOPCOUNT; ++i)
            last = base + i;     // last logical iter is i == LOOPCOUNT-1
    }

    CHECK_EQ(last, base + LOOPCOUNT - 1);
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_lastprivate();

    TEST_PASS();
    return 0;
}
