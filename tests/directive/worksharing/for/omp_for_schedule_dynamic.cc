// xkomp: XFAIL - schedule(dynamic) lowers to __kmpc_dispatch_init_4 /
//        __kmpc_dispatch_next_4, which XKOMP does not export.  Expected to
//        fail to link.  Only the default static schedule is implemented.

#include "common.h"

int
main(void)
{
    static int seen[LOOPCOUNT];
    memset(seen, 0, sizeof(seen));

    #pragma omp parallel num_threads(4) shared(seen)
    {
        #pragma omp for schedule(dynamic, 4)
        for (int i = 0; i < LOOPCOUNT; ++i)
        {
            #pragma omp atomic
            seen[i] += 1;
        }
    }

    for (int i = 0; i < LOOPCOUNT; ++i)
        CHECK_EQ(seen[i], 1);

    TEST_PASS();
    return 0;
}
