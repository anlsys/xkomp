// xkomp: supported  (XKOMP 'access' clause -- REQUIRES the patched LLVM/clang)
//
// Anti- (write-after-read) and output- (write-after-write) dependences over a
// shared region.

#include "common.h"

#define N 64

static int  war_x[N];
static long war_sum;

// write-after-read: the writer must wait for the reader, so the reader observes
// the pre-write values.
static void
test_war(void)
{
    for (int i = 0 ; i < N ; ++i)
        war_x[i] = i;
    war_sum = -1;

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            #pragma omp task access(read: war_x[0:N]) default(none)
            {
                long s = 0;
                for (int i = 0 ; i < N ; ++i)
                    s += war_x[i];
                war_sum = s;
            }

            // zeroes the region -> must run after the reader (write-after-read)
            #pragma omp task access(write: war_x[0:N]) default(none)
            {
                for (int i = 0 ; i < N ; ++i)
                    war_x[i] = 0;
            }

            #pragma omp taskwait
            CHECK_EQ(war_sum, (long)(N - 1) * N / 2);   // sum of 0..N-1
            for (int i = 0 ; i < N ; ++i)
                CHECK_EQ(war_x[i], 0);
        }
    }
}

static int waw_x[N];

// write-after-write: the second writer must run after the first.
static void
test_waw(void)
{
    memset(waw_x, 0, sizeof(waw_x));

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            #pragma omp task access(write: waw_x[0:N]) default(none)
            { for (int i = 0 ; i < N ; ++i) waw_x[i] = 1; }

            #pragma omp task access(write: waw_x[0:N]) default(none)
            { for (int i = 0 ; i < N ; ++i) waw_x[i] = 2; }   // after the first

            #pragma omp taskwait
            for (int i = 0 ; i < N ; ++i)
                CHECK_EQ(waw_x[i], 2);
        }
    }
}

int
main(void)
{
    for (int r = 0 ; r < REPETITIONS ; ++r)
    {
        test_war();
        test_waw();
    }

    TEST_PASS();
    return 0;
}
