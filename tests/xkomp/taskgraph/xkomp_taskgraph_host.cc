// xkomp: supported  (XKOMP taskgraph extension: pragma_omp_taskgraph)
//
// Records a host task graph once (rc==1) and replays it on every subsequent
// encounter of the same graph id.  Each (record or replay) pass must apply the
// task bodies exactly once, so after ITERS passes every element is incremented
// ITERS times.  The default flags carry an implicit taskgroup, so each pass has
// completed by the time pragma_omp_taskgraph returns.

#include "common.h"

#include <xkomp/xkomp.h>
#include <xkomp/xkomp++.h>

XKRT_NAMESPACE_USE;

#define NB    8
#define ITERS 5

int
main(void)
{
    int v[NB];
    for (int b = 0; b < NB; ++b)
        v[b] = b;

    int it;

    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        {
            for (it = 0; it < ITERS; ++it)
            {
                constexpr xkomp_taskgraph_id_t    gid   = 0;
                constexpr xkomp_taskgraph_flags_t flags = XKOMP_TASKGRAPH_FLAG_NONE;

                pragma_omp_taskgraph(gid, flags, [&] (void)
                {
                    for (int b = 0; b < NB; ++b)
                    {
                        // distinct token per block -> independent tasks
                        #pragma omp task depend(out: v[b]) firstprivate(b) shared(v, it) default(none)
                        {
                            v[b] += 1;
                            printf("Executing task %d of iter %d\n", b, it);
                        }
                    }
                });
            }
        }
    }

    for (int b = 0; b < NB; ++b)
        CHECK_EQ(v[b], b + ITERS);

    TEST_PASS();
    return 0;
}
