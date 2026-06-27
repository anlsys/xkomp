// xkomp: supported  (untied tasks)
//
// An untied task may resume on a different thread than the one it started on.
// Functionally the results must be identical to tied tasks; this checks the
// untied flag is accepted and the tasks run correctly.

#include "common.h"

static void
test_untied(void)
{
    static int done[NUM_TASKS];
    memset(done, 0, sizeof(done));
    int executed = 0;

    #pragma omp parallel num_threads(4) shared(done, executed)
    {
        #pragma omp single
        {
            for (int i = 0; i < NUM_TASKS; ++i)
            {
                #pragma omp task untied firstprivate(i) shared(done, executed)
                {
                    done[i] = 1;
                    #pragma omp atomic
                    executed += 1;
                }
            }
            #pragma omp taskwait

            for (int i = 0; i < NUM_TASKS; ++i)
                CHECK_EQ(done[i], 1);
            CHECK_EQ(executed, NUM_TASKS);
        }
    }
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_untied();

    TEST_PASS();
    return 0;
}
