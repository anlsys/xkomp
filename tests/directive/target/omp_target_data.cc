// xkomp: supported  (structured `target data` region + `target update` inside)
//
// NOTE: requires the offload runtime (libomptarget); guarded by XKOMP_TESTS_TARGET.
//
// Complements the explicit enter/exit-data form in omp_target_map.cc with the
// scoped `target data` region, and checks a mid-region device->host update.

#include "common.h"

#define N        1024
#define DEVICE   0

int
main(void)
{
    double * x = (double *) malloc(sizeof(double) * N);
    CHECK(x != NULL);
    for (int i = 0; i < N; ++i)
        x[i] = (double) i;

    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        {
            #pragma omp target data map(tofrom: x[0:N]) device(DEVICE)
            {
                #pragma omp target teams distribute parallel for device(DEVICE)
                for (int i = 0; i < N; ++i)
                    x[i] = x[i] + 5.0;

                // pull the device copy back mid-region and check it
                #pragma omp target update from(x[0:N]) device(DEVICE)
                for (int i = 0; i < N; ++i)
                    CHECK_NEAR(x[i], i + 5.0, 1e-9);
            }
            // leaving the region flushes the tofrom mapping back to the host
        }
    }

    for (int i = 0; i < N; ++i)
        CHECK_NEAR(x[i], i + 5.0, 1e-9);

    free(x);

    TEST_PASS();
    return 0;
}
