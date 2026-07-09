// xkomp: supported  (__kmpc_for_static_init_4u and __kmpc_for_static_init_8u)
//
// The signed 32-bit path (__kmpc_for_static_init_4) is covered by omp_for.cc.
// This test exercises the *unsigned* worksharing entry points, which are
// implemented in src/kmp/for.cc but otherwise untested:
//   - `unsigned`            -> __kmpc_for_static_init_4u
//   - `unsigned long long`  -> __kmpc_for_static_init_8u

#include "common.h"

static void
test_u32(void)
{
    static int seen[LOOPCOUNT];
    memset(seen, 0, sizeof(seen));

    #pragma omp parallel num_threads(4) shared(seen)
    {
        #pragma omp for
        for (unsigned i = 0; i < (unsigned) LOOPCOUNT; ++i)
        {
            #pragma omp atomic
            seen[i] += 1;
        }
    }

    for (int i = 0; i < LOOPCOUNT; ++i)
        CHECK_EQ(seen[i], 1);
}

static void
test_u64(void)
{
    static int seen[LOOPCOUNT];
    memset(seen, 0, sizeof(seen));

    #pragma omp parallel num_threads(4) shared(seen)
    {
        #pragma omp for
        for (unsigned long long i = 0; i < (unsigned long long) LOOPCOUNT; ++i)
        {
            #pragma omp atomic
            seen[(int) i] += 1;
        }
    }

    for (int i = 0; i < LOOPCOUNT; ++i)
        CHECK_EQ(seen[i], 1);
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
    {
        test_u32();
        test_u64();
    }

    TEST_PASS();
    return 0;
}
