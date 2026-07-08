// xkomp: XFAIL - `task if(false)` produces an undeferred task via
//        __kmpc_omp_task_begin_if0 / __kmpc_omp_task_complete_if0, which XKOMP
//        does not export.  Expected to fail to link.

#include "common.h"

int
main(void)
{
    volatile int cond = 0;   // runtime-false -> undeferred (if0) task path
    int x = 0;

    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        {
            #pragma omp task if(cond) shared(x) default(none)
            x = 7;

            #pragma omp taskwait
            CHECK_EQ(x, 7);
        }
    }

    TEST_PASS();
    return 0;
}
