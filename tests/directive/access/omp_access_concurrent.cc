// xkomp: supported  (XKOMP 'access' clause -- REQUIRES the patched LLVM/clang)
//
// The `concurrent` modifier (analogous to depend's inoutset) lets several
// writers over the same region run concurrently with respect to each other,
// while still ordering a subsequent reader after all of them.
//
// (Mirrors iwomp26/access-examples/concurrent.cc.)

#include "common.h"

#define N 64

static void
test_concurrent(void)
{
    static int x[N];
    memset(x, 0, sizeof(x));

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            // two concurrent writers on the same region; they touch disjoint
            // elements (even / odd) and may execute in parallel
            #pragma omp task access(concurrentwrite: x[0:N]) default(none)
            { for (int i = 0 ; i < N ; i += 2) x[i] = i; }

            #pragma omp task access(concurrentwrite: x[0:N]) default(none)
            { for (int i = 1 ; i < N ; i += 2) x[i] = i; }

            // reader: after BOTH concurrent writers (write -> read)
            #pragma omp task access(read: x[0:N]) default(none)
            { for (int i = 0 ; i < N ; ++i) CHECK_EQ(x[i], i); }

            #pragma omp taskwait
            for (int i = 0 ; i < N ; ++i)
                CHECK_EQ(x[i], i);
        }
    }
}

int
main(void)
{
    for (int r = 0 ; r < REPETITIONS ; ++r)
        test_concurrent();

    TEST_PASS();
    return 0;
}
