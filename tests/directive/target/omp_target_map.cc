// xkomp: supported  (target / map / target teams distribute parallel for)
//
// NOTE: target tests require the offload runtime (libomptarget) from the
// patched LLVM.  They are guarded behind the XKOMP_TESTS_TARGET CMake option.
// With no accelerator present, device 0 is the (host) initial device and the
// regions run as host fallback; the data checks still hold.

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
            // explicit map lifecycle + update + compute on the device
            #pragma omp target enter data map(alloc: x[0:N]) device(DEVICE)

            #pragma omp target update to(x[0:N]) device(DEVICE)

            #pragma omp target teams distribute parallel for device(DEVICE)
            for (int i = 0; i < N; ++i)
                x[i] = x[i] * 2.0;

            #pragma omp target update from(x[0:N]) device(DEVICE)

            for (int i = 0; i < N; ++i)
                CHECK_NEAR(x[i], i * 2.0, 1e-9);

            #pragma omp target exit data map(release: x[0:N]) device(DEVICE)
        }
    }

    // a second, self-contained map(tofrom: ...) form
    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        {
            #pragma omp target teams distribute parallel for map(tofrom: x[0:N]) device(DEVICE)
            for (int i = 0; i < N; ++i)
                x[i] = x[i] + 1.0;
        }
    }
    for (int i = 0; i < N; ++i)
        CHECK_NEAR(x[i], i * 2.0 + 1.0, 1e-9);

    free(x);

    TEST_PASS();
    return 0;
}
