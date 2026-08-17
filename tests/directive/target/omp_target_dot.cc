// xkomp: supported  (asynchronous GPU reduction offloaded as a target task)
//
// NOTE: needs the offload runtime (libomptarget); guarded by XKOMP_TESTS_TARGET.
//
// Minimal unit-test for a GPU *reduction* offloaded as an asynchronous (nowait)
// target task -- a dot product.  Each
//   `target teams distribute parallel for reduction(+:acc) ... nowait`
// reduces partial sums across teams through the per-launch device reduction
// buffer + kernel-launch-environment (KLE) that XKOMP allocates for reduction
// kernels; the final scalar is copied back as part of the async target task and
// joined with `taskwait` before the host reads it.
//
// Two independent dot products are launched back-to-back with distinct depend
// tokens (so the runtime may run them concurrently -- each therefore needs its
// OWN reduction buffer/KLE) and joined by a single `taskwait`.  The whole thing
// is repeated REPS times to shake out races.  Every device result must match the
// host reference computed in the same precision.

#include "common.h"

#include <stdlib.h>

#define N    (1 << 16)   // large enough to span many teams -> real teams reduction
#define REPS 8

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

    double * x = (double *) malloc(sizeof(double) * N);
    double * y = (double *) malloc(sizeof(double) * N);
    double * z = (double *) malloc(sizeof(double) * N);
    CHECK(x && y && z);

    // Varied (but bounded) data so the reduction actually sums distinct terms;
    // the host reference is accumulated in the same double precision.
    double ref_xy = 0.0;
    double ref_xz = 0.0;
    for (int i = 0; i < N; ++i)
    {
        x[i] = (double)(i % 17) * 0.5  + 1.0;
        y[i] = (double)(i % 13) * 0.25 + 0.5;
        z[i] = (double)(i %  7) * 0.75 - 0.5;
        ref_xy += x[i] * y[i];
        ref_xz += x[i] * z[i];
    }

    // Resident, read-only operands for the whole run.
    #pragma omp target enter data map(to: x[0:N], y[0:N], z[0:N])

    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        {
            for (int r = 0; r < REPS; ++r)
            {
                double dot_xy = 0.0;
                double dot_xz = 0.0;

                // Two independent async reductions: distinct depend tokens, so
                // they may overlap and each needs its own device reduction
                // buffer / KLE.
                // The operands are resident on the device (target enter data
                // above), so map(present:) asserts their presence without copying.
                #pragma omp target teams distribute parallel for \
                        reduction(+: dot_xy) map(present: x[0:N], y[0:N]) \
                        depend(inout: dot_xy) nowait
                for (int i = 0; i < N; ++i)
                    dot_xy += x[i] * y[i];

                #pragma omp target teams distribute parallel for \
                        reduction(+: dot_xz) map(present: x[0:N], z[0:N]) \
                        depend(inout: dot_xz) nowait
                for (int i = 0; i < N; ++i)
                    dot_xz += x[i] * z[i];

                // Join both asynchronous target tasks before reading the results.
                #pragma omp taskwait

                CHECK_NEAR(dot_xy, ref_xy, 1e-9);
                CHECK_NEAR(dot_xz, ref_xz, 1e-9);
            }
        }
    }

    #pragma omp target exit data map(release: x[0:N], y[0:N], z[0:N])

    free(x);
    free(y);
    free(z);

    TEST_PASS();
    return 0;
}
