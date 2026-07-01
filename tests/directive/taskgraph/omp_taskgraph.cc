// xkomp: XFAIL - the directive-based `#pragma omp taskgraph` construct (OpenMP
//        6.0) is not yet lowered by the compiler in use.  Until then the
//        taskgraph feature is only reachable through the xkomp++.h C++ interface
//        (see tests/xkomp++/taskgraph/omp_taskgraph.cc, which exercises the same
//        record/replay logic via pragma_omp_taskgraph()).
//
//        This test tracks the directive form: once the compiler emits it, the
//        test compiles, runs and flips to PASS.  (The exact clause spelling --
//        here `graph_id(...)`, mirroring include/xkomp/taskgraph.h -- may need to
//        be adjusted to match the final compiler/spec support.)

#include "common.h"

#define NB    8
#define ITERS 5

int
main(void)
{
    int v[NB];
    for (int b = 0; b < NB; ++b)
        v[b] = b;

    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        {
            // Encounter the same taskgraph id ITERS times: the runtime records it
            // on the first encounter and replays it on the following ones.  Each
            // pass applies every task body exactly once.
            for (int it = 0; it < ITERS; ++it)
            {
                #pragma omp taskgraph graph_id(0)
                {
                    for (int b = 0; b < NB; ++b)
                    {
                        #pragma omp task depend(out: v[b]) firstprivate(b) shared(v)
                        v[b] += 1;
                    }
                }
            }
        }
    }

    for (int b = 0; b < NB; ++b)
        CHECK_EQ(v[b], b + ITERS);

    TEST_PASS();
    return 0;
}
