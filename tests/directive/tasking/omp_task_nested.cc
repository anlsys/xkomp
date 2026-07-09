// xkomp: supported  (nested task generation: a task spawns child tasks)
//
// Divide-and-conquer tree sum: every internal node spawns two child tasks and
// joins them with taskwait.  Exercises recursive task creation and per-level
// taskwait, which the flat omp_task.cc does not.

#include "common.h"

#define N      256
#define THRESH 8

static long
tree_sum(const long * a, int lo, int hi)
{
    if (hi - lo <= THRESH)
    {
        long s = 0;
        for (int i = lo; i < hi; ++i)
            s += a[i];
        return s;
    }

    const int mid = (lo + hi) / 2;
    long left = 0, right = 0;

    #pragma omp task shared(left) firstprivate(a, lo, mid) default(none)
    left = tree_sum(a, lo, mid);

    #pragma omp task shared(right) firstprivate(a, mid, hi) default(none)
    right = tree_sum(a, mid, hi);

    #pragma omp taskwait
    return left + right;
}

int
main(void)
{
    static long a[N];
    for (int i = 0; i < N; ++i)
        a[i] = i;

    const long expected = (long) N * (N - 1) / 2;

    for (int r = 0; r < REPETITIONS; ++r)
    {
        long result = -1;

        #pragma omp parallel num_threads(4) shared(result, a)
        {
            #pragma omp single
            result = tree_sum(a, 0, N);
        }

        CHECK_EQ(result, expected);
    }

    TEST_PASS();
    return 0;
}
