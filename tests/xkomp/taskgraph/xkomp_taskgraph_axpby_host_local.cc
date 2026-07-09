// xkomp: supported  (taskgraph over CPU tasks: AXPBY = scale then axpy, local arrays)
//
// Local-array variant of xkomp_taskgraph_axpby_host.cc: the per-block scale->axpy
// chain (a fusion candidate) over `float[N]` locals captured `shared`, exercising
// the leaf-form SHARED_ADDR capture path together with loop fusion (the globals
// variant reaches x/y as direct global references instead of captures).  Per pass
// y <- alpha*x + beta*y; validated against a serial host reference.
#include "common.h"
#include <xkomp/xkomp.h>
#include <xkomp/xkomp++.h>
XKRT_NAMESPACE_USE;
#define NB 8
#define BS 64
#define N  (NB * BS)
#define ITERS 5
#define ALPHA 0.7f
#define BETA  0.5f
int
main(void)
{
    float x[N];
    float y[N];
    float ref[N];
    for (int i = 0; i < N; ++i) { x[i] = (float) i + 0.1f; y[i] = (float) i + 1.0f; ref[i] = y[i]; }
    for (int it = 0; it < ITERS; ++it)
        for (int i = 0; i < N; ++i) { ref[i] = BETA * ref[i]; ref[i] = ALPHA * x[i] + ref[i]; }
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
                        const int j = b * BS;
                        #pragma omp task depend(out: y[j]) firstprivate(j) shared(y) default(none)   // scale
                        for (int i = 0; i < BS; ++i)
                            y[i + j] = BETA * y[i + j];
                        #pragma omp task depend(out: y[j]) firstprivate(j) shared(x, y) default(none)   // axpy
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
