// xkomp: XFAIL - schedule(static, chunk) is lowered with schedtype
//        kmp_sch_static_chunked (33), but __kmpc_for_static_init_4 asserts
//        schedtype == kmp_sch_static (34).  Links, then aborts at runtime.
//        Only the unspecified (balanced) static schedule is implemented.

#include "common.h"

int
main(void)
{
    static int seen[LOOPCOUNT];
    memset(seen, 0, sizeof(seen));

    #pragma omp parallel num_threads(4) shared(seen)
    {
        #pragma omp for schedule(static, 4)
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
