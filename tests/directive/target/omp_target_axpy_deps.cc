// xkomp: supported  (async multi-device offload; dependences over the data)
//
// NOTE: needs the offload runtime (libomptarget); guarded by XKOMP_TESTS_TARGET.
//
// Same idea as omp_target_axpy.cc, but the H2D -> kernel -> D2H ordering is
// expressed with dependences over the actual arrays (the idiomatic standard
// OpenMP style) rather than a dedicated per-device token:
//
//   update to(x)   depend(out: x)
//   update to(y)   depend(out: y)
//   kernel         depend(in: x) depend(inout: y)   (runs after both H2D)
//   update from(y) depend(in: y)                     (runs after the kernel)
//
// One AXPY pass (y <- alpha*x + y); distinct device blocks are independent.

#include "common.h"

#include <stdlib.h>

#define BS 64

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

    const size_t size  = (size_t) BS * (size_t) ndevices;
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

            for (int d = 0; d < ndevices; ++d)
            {
                const int j = d * BS;

                #pragma omp target update to(x[j:BS]) device(d) depend(out: x[j:BS]) nowait
                #pragma omp target update to(y[j:BS]) device(d) depend(out: y[j:BS]) nowait

                #pragma omp target teams distribute parallel for device(d) nowait \
                        depend(in: x[j:BS]) depend(inout: y[j:BS]) firstprivate(alpha, j)
                for (int i = 0; i < BS; ++i)
                    y[i + j] = alpha * x[i + j] + y[i + j];

                #pragma omp target update from(y[j:BS]) device(d) depend(in: y[j:BS]) nowait
            }

            #pragma omp taskwait

            for (int d = 0; d < ndevices; ++d)
            {
                #pragma omp target exit data map(release: x[d*BS:BS]) device(d)
                #pragma omp target exit data map(release: y[d*BS:BS]) device(d)
            }
        }
    }

    for (size_t i = 0; i < size; ++i)
        CHECK_NEAR(y[i], y0[i] + alpha * x[i], 1e-3);

    free(x);
    free(y);
    free(y0);

    TEST_PASS();
    return 0;
}
