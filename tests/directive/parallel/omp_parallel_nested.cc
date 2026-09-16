// xkomp: supported  (nested regions are serialized -> team of 1)
//
// Nested parallelism is implementation-defined and disabled by default, so an
// inner `parallel` runs with a one-thread team.  XKOMP serializes the nested
// region in the encountering thread (src/xkomp/parallel.cc).  This test pins
// that conformant behaviour and checks the inner regions all execute.

#include "common.h"

static void
test_nested(void)
{
    static int inner_runs[XKOMP_TEST_MAXT];
    static int inner_size_ok[XKOMP_TEST_MAXT];
    memset(inner_runs, 0, sizeof(inner_runs));
    memset(inner_size_ok, 0, sizeof(inner_size_ok));

    int outer_size = 0;

    #pragma omp parallel num_threads(4) shared(inner_runs, inner_size_ok, outer_size)
    {
        const int otid = omp_get_thread_num();
        if (otid == 0)
            outer_size = omp_get_num_threads();

        // each outer thread opens its own (serialized) inner region
        #pragma omp parallel num_threads(3)
        {
            const int isize = omp_get_num_threads();

            // nested parallelism disabled -> a single-thread inner team
            if (isize == 1 && omp_get_thread_num() == 0)
                inner_size_ok[otid] = 1;

            #pragma omp atomic
            inner_runs[otid] += 1;
        }
    }

    CHECK(outer_size >= 1);
    for (int i = 0; i < outer_size; ++i)
    {
        CHECK_EQ(inner_size_ok[i], 1);  // inner team had exactly 1 thread
        CHECK_EQ(inner_runs[i], 1);     // inner body ran once for that outer thread
    }
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_nested();

    TEST_PASS();
    return 0;
}
