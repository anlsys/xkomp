// xkomp: supported  (taskgraph + target: AXPBY = scale then axpy, record/replay)
//
// NOTE: requires the offload runtime (libomptarget); guarded by XKOMP_TESTS_TARGET.
//
// Blocked AXPBY: y <- alpha*x + beta*y, expressed as TWO chained device kernels
// per block:
//
//     (1) scale:  y[i] = beta * y[i]              <- predecessor
//     (2) axpy:   y[i] = alpha * x[i] + y[i]      <- successor (depends on scale)
//
// The scale kernel is a predecessor of the axpy kernel (both serialized on the
// per-device token), which is the point of this test: two consecutive kernels
// over the same y block within a recorded taskgraph -- a candidate for kernel
// fusion during taskgraph optimization/replay (to be exercised later).
//
// Otherwise it mirrors omp_taskgraph_axpy.cc: the per-iteration schedule is
// recorded once and replayed, ITERS times in total, and the result is validated
// against an independent serial host computation of the same recurrence.

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
    const float  beta  = 0.5f;

    float * x   = (float *) malloc(sizeof(float) * size);
    float * y   = (float *) malloc(sizeof(float) * size);
    float * ref = (float *) malloc(sizeof(float) * size);   // serial host reference
    CHECK(x && y && ref);
    for (size_t i = 0; i < size; ++i)
    {
        x[i]   = (float) i + 0.1f;
        y[i]   = (float) i + 1.0f;
        ref[i] = y[i];
    }

    // host reference: same recurrence, same two-step (scale then axpy) per pass
    for (int it = 0; it < ITERS; ++it)
        for (size_t i = 0; i < size; ++i)
        {
            ref[i] = beta * ref[i];            // scale
            ref[i] = alpha * x[i] + ref[i];    // axpy
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

                        #pragma omp target update to(x[j:BS]) device(d) depend(out: deps[d]) nowait
                        #pragma omp target update to(y[j:BS]) device(d) depend(out: deps[d]) nowait

                        // (1) scale: y = beta*y  -- predecessor of the axpy below
                        #pragma omp target teams distribute parallel for device(d) \
                                depend(out: deps[d]) nowait firstprivate(beta, j)
                        for (int i = 0; i < BS; ++i)
                            y[i + j] = beta * y[i + j];

                        // (2) axpy: y = alpha*x + y  -- successor (ordered after scale)
                        #pragma omp target teams distribute parallel for device(d) \
                                depend(out: deps[d]) nowait firstprivate(alpha, j)
                        for (int i = 0; i < BS; ++i)
                            y[i + j] = alpha * x[i + j] + y[i + j];

                        #pragma omp target update from(y[j:BS]) device(d) depend(out: deps[d]) nowait
                    }
                }
                );
            }

            for (int d = 0; d < ndevices; ++d)
            {
                #pragma omp target exit data map(release: x[d*BS:BS]) device(d)
                #pragma omp target exit data map(release: y[d*BS:BS]) device(d)
            }
        }
    }

    for (size_t i = 0; i < size; ++i)
        CHECK_NEAR(y[i], ref[i], 1e-3);

    free(x);
    free(y);
    free(ref);

    TEST_PASS();
    return 0;
}
