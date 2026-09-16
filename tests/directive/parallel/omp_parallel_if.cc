// xkomp: XFAIL - the false branch of `parallel if(...)` lowers to
//        __kmpc_serialized_parallel / __kmpc_end_serialized_parallel, which
//        XKOMP does not export.  Expected to fail to link.

#include "common.h"

int
main(void)
{
    volatile int cond = 0;   // runtime-false -> serialized parallel path
    int who = -1;
    int nth = -1;

    #pragma omp parallel if(cond) num_threads(4) shared(who, nth)
    {
        who = omp_get_thread_num();
        nth = omp_get_num_threads();
    }

    CHECK_EQ(who, 0);
    CHECK_EQ(nth, 1);   // serialized -> single thread

    TEST_PASS();
    return 0;
}
