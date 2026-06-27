// xkomp: XFAIL - `taskgroup` needs __kmpc_taskgroup / __kmpc_end_taskgroup,
//        which XKOMP does not export.  Expected to fail to link.

#include "common.h"

static void
test_taskgroup(void)
{
    static int done[NUM_TASKS];
    memset(done, 0, sizeof(done));

    #pragma omp parallel num_threads(4) shared(done)
    {
        #pragma omp single
        {
            // taskgroup waits for all descendant tasks at its end (deep sync),
            // unlike taskwait which only waits for direct children.
            #pragma omp taskgroup
            {
                for (int i = 0; i < NUM_TASKS; ++i)
                {
                    #pragma omp task firstprivate(i) shared(done)
                    done[i] = 1;
                }
            }

            for (int i = 0; i < NUM_TASKS; ++i)
                CHECK_EQ(done[i], 1);
        }
    }
}

int
main(void)
{
    test_taskgroup();
    TEST_PASS();
    return 0;
}
