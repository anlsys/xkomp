// xkomp: supported  (parallel + for + private partials + atomic combine)
//
// XKOMP does not implement the `reduction` clause (see
// worksharing/for/omp_for_reduction.cc, XFAIL).  This is the standard-compliant
// hand-rolled equivalent: each thread accumulates a private partial over its
// share of a worksharing loop, then combines it atomically.

#include "common.h"

static void
test_manual_reduction(void)
{
    static double a[LOOPCOUNT];
    for (int i = 0; i < LOOPCOUNT; ++i)
        a[i] = 0.5 * i + 1.0;

    double total = 0.0;

    #pragma omp parallel num_threads(4) shared(a, total)
    {
        double local = 0.0;   // private partial

        #pragma omp for nowait
        for (int i = 0; i < LOOPCOUNT; ++i)
            local += a[i];

        #pragma omp atomic
        total += local;       // combine partials
    }

    double expected = 0.0;
    for (int i = 0; i < LOOPCOUNT; ++i)
        expected += a[i];

    CHECK_NEAR(total, expected, 1e-9);
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_manual_reduction();

    TEST_PASS();
    return 0;
}
