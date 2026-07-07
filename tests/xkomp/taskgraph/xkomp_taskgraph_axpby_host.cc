// xkomp: supported  (taskgraph over CPU tasks: AXPBY = scale then axpy)
//
// CPU-tasking counterpart of omp_taskgraph_axpby.cc (no target / offload).  Per
// block, a scale task (y = beta*y) is a predecessor of an axpy task
// (y = alpha*x + y), ordered through the shared y-block handle -- two consecutive
// host tasks over the same block within a recorded taskgraph (a fusion
// candidate).  Per pass y <- alpha*x + beta*y; validated against a serial host
// reference running the same recurrence.

#include "common.h"

#include <xkomp/xkomp.h>
#include <xkomp/xkomp++.h>

XKRT_NAMESPACE_USE;

#define NB    16
#define BS    64
#define N     (NB * BS)
#define ITERS 16
#define ALPHA 0.7f
#define BETA  0.5f

static float x[N];
static float y[N];
static float ref[N];

int
main(void)
{
    for (int i = 0; i < N; ++i)
    {
        x[i]   = (float) i + 0.1f;
        y[i]   = (float) i + 1.0f;
        ref[i] = y[i];
    }

    // serial host reference: same two-step recurrence per pass
    for (int it = 0; it < ITERS; ++it)
        for (int i = 0; i < N; ++i)
        {
            ref[i] = BETA * ref[i];             // scale
            ref[i] = ALPHA * x[i] + ref[i];     // axpy
        }

    #pragma omp parallel
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
                        const int j = b * BS;

                        // (1) scale: y = beta*y  -- predecessor
                        #pragma omp task depend(out: y[j]) firstprivate(j)
                        for (int i = 0; i < BS; ++i)
                            y[i + j] = BETA * y[i + j];

                        // (2) axpy: y = alpha*x + y  -- after the scale (same &y[j])
                        #pragma omp task depend(out: y[j]) firstprivate(j)
                        for (int i = 0; i < BS; ++i)
                            y[i + j] = ALPHA * x[i + j] + y[i + j];
                    }
                });
            }
        }
    }

    for (int i = 0; i < N; ++i)
        CHECK_NEAR(y[i], ref[i], 1e-3);

    TEST_PASS();
    return 0;
}
