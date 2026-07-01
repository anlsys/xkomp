// xkomp: supported  (dependent CPU tasks, plain OpenMP -- no taskgraph, no target)
//
// Iterative blocked AXPY (y <- alpha*x + y) computed with standard OpenMP tasks
// and dependences.  The array is split into NB blocks; each block is updated by
// one task per iteration, and successive iterations of the same block are chained
// through `depend(inout: y[block])` (a per-block dependence).  Different blocks
// are independent and run concurrently.  After ITERS passes the result must be
// y == y0 + ITERS*alpha*x.
//
// This is the plain-tasking analogue of the taskgraph/target axpy tests.

#include "common.h"

#define NB    8
#define BS    64
#define N     (NB * BS)
#define ITERS 5
#define ALPHA 0.7f

static float x[N];
static float y[N];
static float y0[N];

int
main(void)
{
    for (int i = 0; i < N; ++i)
    {
        x[i]  = (float) i + 0.1f;
        y[i]  = (float) i + 1.0f;
        y0[i] = y[i];
    }

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            for (int it = 0; it < ITERS; ++it)
            {
                for (int b = 0; b < NB; ++b)
                {
                    const int j = b * BS;
                    // depends (inout) on this block => chains with the same block's
                    // task from the previous iteration; blocks stay independent
                    #pragma omp task depend(inout: y[j]) firstprivate(j)
                    for (int i = 0; i < BS; ++i)
                        y[i + j] = ALPHA * x[i + j] + y[i + j];
                }
            }
            #pragma omp taskwait
        }
    }

    for (int i = 0; i < N; ++i)
        CHECK_NEAR(y[i], y0[i] + ITERS * ALPHA * x[i], 1e-3);

    TEST_PASS();
    return 0;
}
