// xkomp: supported  (async multi-device offload with plain OpenMP directives)
//
// NOTE: needs the offload runtime (libomptarget); guarded by XKOMP_TESTS_TARGET.
//
// Iterative blocked AXPY (y <- alpha*x + y) offloaded to all available devices
// using ONLY standard OpenMP directives -- no taskgraph, no access clause.  It is
// the taskgraph-free analogue of xkomp++/taskgraph/omp_taskgraph_axpy.cc:
// `target ... nowait depend(...)` turns each offload op into a task, a per-device
// scalar token serializes that device's H2D / kernels / D2H pipeline, distinct
// devices run concurrently, and `taskwait` joins before the host reads back.
//
// After ITERS kernel passes the host y must equal yinit + ITERS*alpha*x.

#include "common.h"

#include <stdlib.h>

#define BS    64
#define ITERS 3

int
main(void)
{
    const int ndevices = omp_get_num_devices();
    if (ndevices < 1)
    {
        printf("[xkomp-test] %s: no devices available, skipping\n", __FILE__);
        TEST_PASS();
        return 0;
    }
    printf("[xkomp-test] %s: %d devices available\n", __FILE__, ndevices);

    const size_t size  = (size_t) BS * (size_t) ndevices;   // divides evenly
    const float  alpha = 0.7f;

    float * x  = (float *) malloc(sizeof(float) * size);
    float * y  = (float *) malloc(sizeof(float) * size);
    float * yinit = (float *) malloc(sizeof(float) * size);
    char  * dep = (char *) malloc((size_t) ndevices);       // per-device dep tokens
    CHECK(x && y && yinit && dep);
    for (size_t i = 0; i < size; ++i)
    {
        x[i]  = (float) i + 0.1f;
        y[i]  = (float) i + 1.0f;
        yinit[i] = y[i];
    }

    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        {
            // allocate a device replica for each block
            for (int d = 0; d < ndevices; ++d)
            {
                #pragma omp target enter data map(alloc: x[d*BS:BS]) device(d)
                #pragma omp target enter data map(alloc: y[d*BS:BS]) device(d)
            }

            // async pipeline per device, all ordered through dep[d]:
            //   H2D(x) -> H2D(y) -> ITERS x kernel -> D2H(y)
            for (int d = 0; d < ndevices; ++d)
            {
                const int j = d * BS;

                #pragma omp target update to(x[j:BS]) device(d) depend(out: dep[d]) nowait
                #pragma omp target update to(y[j:BS]) device(d) depend(out: dep[d]) nowait

                for (int it = 0; it < ITERS; ++it)
                {
                    #pragma omp target teams distribute parallel for device(d) \
                            depend(out: dep[d]) nowait firstprivate(alpha, j)
                    for (int i = 0; i < BS; ++i)
                        y[i + j] = alpha * x[i + j] + y[i + j];
                }

                #pragma omp target update from(y[j:BS]) device(d) depend(out: dep[d]) nowait
            }

            // join the asynchronous pipelines of every device
            #pragma omp taskwait

            for (int d = 0; d < ndevices; ++d)
            {
                #pragma omp target exit data map(release: x[d*BS:BS]) device(d)
                #pragma omp target exit data map(release: y[d*BS:BS]) device(d)
            }
        }
    }

    for (size_t i = 0; i < size; ++i)
        CHECK_NEAR(y[i], yinit[i] + ITERS * alpha * x[i], 1e-3);

    free(x);
    free(y);
    free(yinit);
    free(dep);

    TEST_PASS();
    return 0;
}
