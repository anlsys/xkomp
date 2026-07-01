// xkomp: supported  (__kmpc_omp_task_with_deps : in / out)
//
// A producer/consumer chain over two scalar tokens.  The asserts inside the
// tasks hold only if the runtime honours the in/out ordering (they hold
// regardless of whether the tasks actually run in parallel).
//
// (`out` is used instead of `inout`; OpenMP treats them identically for
//  dependence computation.)

#include "common.h"

static void
test_depend(void)
{
    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            int x = 0;
            int y = 0;

            // T1: produce x
            #pragma omp task depend(out: x) shared(x)
            {
                CHECK_EQ(x, 0);
                x = 1;
            }

            // T2: read x, produce y   (must run after T1)
            #pragma omp task depend(in: x) depend(out: y) shared(x, y)
            {
                CHECK_EQ(x, 1);
                CHECK_EQ(y, 0);
                y = 2;
            }

            // T3: update x, read y     (must run after T1 and T2)
            #pragma omp task depend(out: x) depend(in: y) shared(x, y)
            {
                CHECK_EQ(x, 1);
                CHECK_EQ(y, 2);
                x = 3;
            }

            // T4: read final x         (must run after T3)
            #pragma omp task depend(in: x) shared(x)
            {
                CHECK_EQ(x, 3);
            }

            #pragma omp taskwait
            CHECK_EQ(x, 3);
            CHECK_EQ(y, 2);
        }
    }
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_depend();

    TEST_PASS();
    return 0;
}
