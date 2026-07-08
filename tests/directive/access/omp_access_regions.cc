// xkomp: supported  (XKOMP 'access' clause -- REQUIRES the patched LLVM/clang)
//
// The defining feature of access over depend: dependences are over *regions*
// (intervals).  Disjoint regions are independent; *partially overlapping*
// regions are ordered -- something depend cannot express (it requires identical
// or disjoint storage).

#include "common.h"

#define N 64

// (a) disjoint write regions are independent; a reader of their union waits for
//     all of them.
static void
test_disjoint(void)
{
    static int x[N];
    memset(x, 0, sizeof(x));

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            #pragma omp task access(write: x[0:N/2]) default(none)
            { for (int i = 0   ; i < N/2 ; ++i) x[i] = i; }

            #pragma omp task access(write: x[N/2:N/2]) default(none)
            { for (int i = N/2 ; i < N   ; ++i) x[i] = i; }

            // reads the whole region -> intersects both writers -> after both
            #pragma omp task access(read: x[0:N]) default(none)
            { for (int i = 0 ; i < N ; ++i) CHECK_EQ(x[i], i); }

            #pragma omp taskwait
            for (int i = 0 ; i < N ; ++i)
                CHECK_EQ(x[i], i);
        }
    }
}

// (b) partially overlapping write regions are ORDERED.
//     A writes [0, 3N/4) := 1 ; B writes [N/4, N) += 10.
//     They overlap on [N/4, 3N/4), so B must run after A.
static void
test_partial_overlap(void)
{
    static int x[N];
    memset(x, 0, sizeof(x));

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            #pragma omp task access(write: x[0 : 3*N/4]) default(none)
            { for (int i = 0 ; i < 3*N/4 ; ++i) x[i] = 1; }

            // [N/4 : 3N/4] spans [N/4, N); overlaps A on [N/4, 3N/4) -> B after A
            #pragma omp task access(read, write: x[N/4 : 3*N/4]) default(none)
            { for (int i = N/4 ; i < N ; ++i) x[i] += 10; }

            #pragma omp taskwait
            for (int i = 0     ; i < N/4   ; ++i) CHECK_EQ(x[i], 1);   // A only
            for (int i = N/4   ; i < 3*N/4 ; ++i) CHECK_EQ(x[i], 11);  // A then B
            for (int i = 3*N/4 ; i < N     ; ++i) CHECK_EQ(x[i], 10);  // B only
        }
    }
}

int
main(void)
{
    for (int r = 0 ; r < REPETITIONS ; ++r)
    {
        test_disjoint();
        test_partial_overlap();
    }

    TEST_PASS();
    return 0;
}
