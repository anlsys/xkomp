// xkomp: supported  (taskgraph + target combined: record once, replay)
//
// NOTE: requires the offload runtime (libomptarget); guarded by XKOMP_TESTS_TARGET.
//
// Blocked AXPY (y += alpha*x) split across the available devices.  The whole
// per-iteration schedule (h2d, kernel, d2h) is recorded as a taskgraph on the
// first iteration and replayed afterwards.  After ITERS passes the host y must
// equal y0 + ITERS*alpha*x.  Per-device ordering is expressed with a distinct
// scalar token (handle-based dependence); different devices stay independent.

#include "common.h"

#include <xkomp/xkomp.h>
#include <xkomp/xkomp++.h>

XKRT_NAMESPACE_USE;

#define BS    64
#define ITERS 2

int
main(void)
{
    char * deps = (char *) 0x1000;   // per-device dependency tokens (addresses only)

    const int ndevices = omp_get_num_devices();
    if (ndevices < 1)
    {
        printf("[xkomp-test] %s: no devices available, skipping\n", __FILE__);
        TEST_PASS();
        return 0;
    }

    const size_t size  = (size_t) BS * (size_t) ndevices;  // divisible by ndevices
    const float  alpha = 0.7f;

    float * x  = (float *) malloc(sizeof(float) * size);
    float * y  = (float *) malloc(sizeof(float) * size);
    float * y0 = (float *) malloc(sizeof(float) * size);
    CHECK(x && y && y0);
    for (size_t i = 0; i < size; ++i)
    {
        x[i]  = (float) i + 0.1f;
        y[i]  = (float) i + 1.0f;
        y0[i] = y[i];
    }

    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        {
            for (int d = 0; d < ndevices; ++d)
            {
                #pragma omp target enter data map(alloc: x[d*BS:BS]) device(d)
                #pragma omp target enter data map(alloc: y[d*BS:BS]) device(d)
            }

            for (int it = 0; it < ITERS; ++it)
            {
                constexpr xkomp_taskgraph_id_t    gid   = 0;
                constexpr xkomp_taskgraph_flags_t flags = XKOMP_TASKGRAPH_FLAG_NONE;

                pragma_omp_taskgraph(gid, flags, [&] (void)
                {
                    for (int d = 0; d < ndevices; ++d)
                    {
                        const int j = d * BS;

                        #pragma omp target update to(x[j:BS]) device(d) depend(inout: deps[d]) nowait
                        #pragma omp target update to(y[j:BS]) device(d) depend(inout: deps[d]) nowait

                        #pragma omp target teams distribute parallel for device(d) \
                                depend(inout: deps[d]) nowait firstprivate(alpha, j)
                        for (int i = 0; i < BS; ++i)
                            y[i + j] = alpha * x[i + j] + y[i + j];

                        #pragma omp target update from(y[j:BS]) device(d) depend(inout: deps[d]) nowait
                    }
                });
            }

            for (int d = 0; d < ndevices; ++d)
            {
                #pragma omp target exit data map(release: x[d*BS:BS]) device(d)
                #pragma omp target exit data map(release: y[d*BS:BS]) device(d)
            }
        }
    }

    for (size_t i = 0; i < size; ++i)
        CHECK_NEAR(y[i], y0[i] + ITERS * alpha * x[i], 1e-3);

    free(x);
    free(y);
    free(y0);

    TEST_PASS();
    return 0;
}
