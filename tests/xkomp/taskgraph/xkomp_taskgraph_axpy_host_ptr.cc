// xkomp: supported  (taskgraph over CPU tasks: blocked AXPY via pointer captures)
//
// Variant of xkomp_taskgraph_axpy_host_local.cc reaching x/y through `float *`
// pointer variables captured `shared`, exercising the leaf-form SHARED_PTR path
// (the pointer is passed by value to the fusable leaf and rebound via a local
// `ptmp`), vs the SHARED_ADDR (array) path in the _local test.  Each block is an
// independent task; after ITERS passes y == yinit + ITERS*alpha*x.
#include "common.h"
#include <xkomp/xkomp.h>
#include <xkomp/xkomp++.h>
XKRT_NAMESPACE_USE;
#define NB 8
#define BS 64
#define N  (NB * BS)
#define ITERS 5
#define ALPHA 0.7f
int
main(void)
{
    float ax[N];
    float ay[N];
    float yinit[N];
    for (int i = 0; i < N; ++i) { ax[i] = (float) i + 0.1f; ay[i] = (float) i + 1.0f; yinit[i] = ay[i]; }
    float *x = ax;   // pointer variables -> shared(x,y) is a SHARED_PTR capture
    float *y = ay;
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
                        #pragma omp task depend(out: y[j]) firstprivate(j) shared(x, y) default(none)
                        for (int i = 0; i < BS; ++i)
                            y[i + j] = ALPHA * x[i + j] + y[i + j];
                    }
                });
            }
        }
    }
    for (int i = 0; i < N; ++i)
        CHECK_NEAR(ay[i], yinit[i] + ITERS * ALPHA * ax[i], 1e-3);
    TEST_PASS();
    return 0;
}
