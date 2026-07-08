// xkomp: supported  (taskgraph over CPU tasks: blocked AXPY, record/replay)
//
// CPU-tasking counterpart of omp_taskgraph_axpy.cc: the same blocked AXPY
// (y <- alpha*x + y) recorded once as a taskgraph and replayed, but computed with
// plain host `#pragma omp task`s instead of `target` offload -- so it needs no
// device / offload runtime.  Each block is an independent task (disjoint y
// region); after ITERS passes y == yinit + ITERS*alpha*x.

#include "common.h"

#include <xkomp/xkomp.h>
#include <xkomp/xkomp++.h>

XKRT_NAMESPACE_USE;

#define NB    8
#define BS    64
#define N     (NB * BS)
#define ITERS 5
#define ALPHA 0.7f

int
main(void)
{
    float x[N];
    float y[N];
    float yinit[N];

    for (int i = 0; i < N; ++i)
    {
        x[i]  = (float) i + 0.1f;
        y[i]  = (float) i + 1.0f;
        yinit[i] = y[i];
    }

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            for (int it = 0; it < ITERS; ++it)
            {
                constexpr xkomp_taskgraph_id_t    gid   = 0;
                constexpr xkomp_taskgraph_flags_t flags = XKOMP_TASKGRAPH_FLAG_NONE;

                pragma_omp_taskgraph(gid, flags, [&] (void)
                {
                    for (int b = 0; b < NB; ++b)
                    {
                        const int j = b * BS;   // block offset; distinct y-block handle
                        #pragma omp task depend(out: y[j]) firstprivate(j) shared(x, y) default(none)
                        for (int i = 0; i < BS; ++i)
                            y[i + j] = ALPHA * x[i + j] + y[i + j];
                    }
                });
            }
        }
    }

    for (int i = 0; i < N; ++i)
        CHECK_NEAR(y[i], yinit[i] + ITERS * ALPHA * x[i], 1e-3);

    TEST_PASS();
    return 0;
}
