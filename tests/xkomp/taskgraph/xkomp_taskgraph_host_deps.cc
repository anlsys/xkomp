// xkomp: supported  (taskgraph records & replays inter-task dependencies)
//
// omp_taskgraph.cc records a set of *independent* tasks.  This one records a
// dependency chain (x += 1  then  x *= 2 on the same token) so the recorded and
// replayed graphs must preserve the ordering, not just re-run the bodies.
//
// Per pass:        x <- (x + 1) * 2
// Starting at 0:   0 -> 2 -> 6 -> 14 ...

#include "common.h"

#include <xkomp/xkomp.h>
#include <xkomp/xkomp++.h>

XKRT_NAMESPACE_USE;

#define ITERS 3

int
main(void)
{
    int x = 0;

    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        {
            for (int it = 0; it < ITERS; ++it)
            {
                constexpr xkomp_taskgraph_id_t    gid   = 0;
                constexpr xkomp_taskgraph_flags_t flags = XKOMP_TASKGRAPH_FLAG_NONE;

                pragma_omp_taskgraph(gid, flags, [&] (void)
                {
                    #pragma omp task depend(out: x) shared(x) default(none)
                    x += 1;

                    #pragma omp task depend(out: x) shared(x) default(none)
                    x *= 2;
                });
            }
        }
    }

    int expected = 0;
    for (int it = 0; it < ITERS; ++it)
        expected = (expected + 1) * 2;

    CHECK_EQ(x, expected);

    TEST_PASS();
    return 0;
}
