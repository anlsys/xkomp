// xkomp: supported  (asynchronous target: update / nowait / depend + taskwait)
//
// NOTE: requires the offload runtime (libomptarget); guarded by XKOMP_TESTS_TARGET.
//
// Chains an asynchronous host->device update, an asynchronous device kernel and
// an asynchronous device->host update with dependences, then joins with taskwait.

#include "common.h"

#define N        1024
#define DEVICE   0

int
main(void)
{
    int * x = (int *) malloc(sizeof(int) * N);
    CHECK(x != NULL);
    for (int i = 0; i < N; ++i)
        x[i] = i;

    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        {
            #pragma omp target enter data map(alloc: x[0:N]) device(DEVICE)

            // h2d, kernel, d2h all deferred (nowait) and ordered through `x`
            #pragma omp target update to(x[0:N]) device(DEVICE) depend(out: x) nowait

            #pragma omp target teams distribute parallel for device(DEVICE) \
                    depend(out: x) nowait
            for (int i = 0; i < N; ++i)
                x[i] = x[i] * 3;

            #pragma omp target update from(x[0:N]) device(DEVICE) depend(in: x) nowait

            #pragma omp taskwait   // join the asynchronous chain

            for (int i = 0; i < N; ++i)
                CHECK_EQ(x[i], i * 3);

            #pragma omp target exit data map(release: x[0:N]) device(DEVICE)
        }
    }

    free(x);

    TEST_PASS();
    return 0;
}
