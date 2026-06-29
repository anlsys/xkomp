// xkomp: XFAIL - collapse(2) merges the two loops into a single iteration space
//        whose induction variable clang typically widens to 64-bit signed,
//        lowering to __kmpc_for_static_init_8 (LOGGER_FATAL "Not impl").
//        Links; aborts at runtime.  (If your clang keeps the 32-bit path this
//        will pass and flip red -> promote it to a supported test.)

#include "common.h"

#define R 40
#define C 25

int
main(void)
{
    static int seen[R * C];
    memset(seen, 0, sizeof(seen));

    #pragma omp parallel num_threads(4) shared(seen)
    {
        #pragma omp for collapse(2)
        for (int i = 0; i < R; ++i)
            for (int j = 0; j < C; ++j)
            {
                #pragma omp atomic
                seen[i * C + j] += 1;
            }
    }

    for (int k = 0; k < R * C; ++k)
        CHECK_EQ(seen[k], 1);

    TEST_PASS();
    return 0;
}
