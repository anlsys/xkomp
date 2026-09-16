// xkomp: supported  (static schedule with non-unit and negative strides)
//
// Exercises both increment branches of team_t::parallel_for_thread_bounds
// (incr > 0 and incr < 0), which the unit-stride omp_for.cc does not.

#include "common.h"

static void
test_positive_stride(void)
{
    static int seen[LOOPCOUNT];
    memset(seen, 0, sizeof(seen));
    int count = 0;

    #pragma omp parallel num_threads(4) shared(seen, count)
    {
        #pragma omp for
        for (int i = 0; i < LOOPCOUNT; i += 3)
        {
            #pragma omp atomic
            seen[i] += 1;
            #pragma omp atomic
            count += 1;
        }
    }

    int expected = 0;
    for (int i = 0; i < LOOPCOUNT; i += 3)
    {
        CHECK_EQ(seen[i], 1);
        ++expected;
    }
    CHECK_EQ(count, expected);
}

static void
test_negative_stride(void)
{
    static int seen[LOOPCOUNT];
    memset(seen, 0, sizeof(seen));

    #pragma omp parallel num_threads(4) shared(seen)
    {
        #pragma omp for
        for (int i = LOOPCOUNT - 1; i >= 0; --i)
        {
            #pragma omp atomic
            seen[i] += 1;
        }
    }

    for (int i = 0; i < LOOPCOUNT; ++i)
        CHECK_EQ(seen[i], 1);
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
    {
        test_positive_stride();
        test_negative_stride();
    }

    TEST_PASS();
    return 0;
}
