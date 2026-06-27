// xkomp: XFAIL - signed 64-bit loop counters lower to __kmpc_for_static_init_8,
//        which is `LOGGER_FATAL("Not impl")` in src/kmp/for.cc.  The symbol
//        links, so this fails at *runtime* (abort), not at link time.

#include "common.h"

int
main(void)
{
    const long long N = LOOPCOUNT;
    long long total = 0;

    #pragma omp parallel num_threads(4) shared(total)
    {
        #pragma omp for
        for (long long i = 0; i < N; ++i)
        {
            #pragma omp atomic
            total += 1;
        }
    }

    CHECK_EQ(total, N);

    TEST_PASS();
    return 0;
}
