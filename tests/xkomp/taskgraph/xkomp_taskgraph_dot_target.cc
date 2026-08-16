// xkomp: supported  (taskgraph + target: dot-product reduction, record/replay)
//
// NOTE: requires the offload runtime (libomptarget); guarded by XKOMP_TESTS_TARGET.
//
// Records a GPU *reduction* into a task dependency graph and replays it.  Each
// recorded pass is the three-kernel dot-product chain used by the Krylov
// solvers' gpu_dot (common/kernels.cpp):
//
//     (1) zero:    result[0] = 0                       (target task)
//     (2) reduce:  result[0] += x[i]*y[i]  for all i   (teams reduction)
//     (3) readback:target update from(result)          (D2H)
//
// serialized through result[0].  The reduction (2) needs a per-launch device
// reduction buffer / kernel-launch-environment (KLE); under record/replay that
// buffer is baked into the recorded command and must PERSIST across replays
// (the device runtime self-resets the reduction counters each launch, so it is
// simply reused).  The zero kernel (1) is what makes replay correct: it resets
// the device accumulator before every reduction, otherwise successive replays
// would keep adding into the resident scalar.
//
// pragma_omp_taskgraph() records+executes on the first pass (rc==1) and replays
// on every later pass (rc>=2), so ITERS passes exercise both.  The host poisons
// result before each pass and checks it afterwards, so a no-op replay (or a
// freed/stale KLE) is caught -- every record and replay must yield the same
// serial host reference.

#include "common.h"

#include <xkomp/xkomp.h>
#include <xkomp/xkomp++.h>

XKRT_NAMESPACE_USE;

#define N     (1 << 16)   // spans many teams -> genuine cross-team reduction
#define ITERS 4           // >= 3: record (rc==1), build+replay (rc==2), replay...

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

    double * x      = (double *) malloc(sizeof(double) * N);
    double * y      = (double *) malloc(sizeof(double) * N);
    double * result = (double *) malloc(sizeof(double));   // device-resident accumulator
    CHECK(x && y && result);

    // Varied (but bounded) data; host reference in the same double precision.
    double ref = 0.0;
    for (int i = 0; i < N; ++i)
    {
        x[i] = (double)(i % 17) * 0.5  + 1.0;
        y[i] = (double)(i % 13) * 0.25 + 0.5;
        ref += x[i] * y[i];
    }

    #pragma omp target enter data map(to:    x[0:N], y[0:N])
    #pragma omp target enter data map(alloc: result[0:1])

    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        {
            for (int it = 0; it < ITERS; ++it)
            {
                constexpr xkomp_taskgraph_id_t    gid   = 0;
                constexpr xkomp_taskgraph_flags_t flags = XKOMP_TASKGRAPH_FLAG_NONE;

                result[0] = -1.0;   // poison: a no-op replay would leave this

                pragma_omp_taskgraph(gid, flags, [&] (void)
                {
                    // (1) reset the device accumulator (predecessor of the reduce)
                    #pragma omp target map(present: result[0:1]) \
                            depend(out: result[0]) nowait
                    {
                        result[0] = 0.0;
                    }

                    // (2) teams reduction into the resident scalar
                    #pragma omp target teams distribute parallel for \
                            reduction(+: result[0]) \
                            map(present: x[0:N], y[0:N], result[0:1]) \
                            depend(in: x[0], y[0]) depend(inout: result[0]) nowait
                    for (int i = 0; i < N; ++i)
                        result[0] += x[i] * y[i];

                    // (3) copy the reduced value back to the host
                    #pragma omp target update from(result[0:1]) \
                            depend(inout: result[0]) nowait
                });

                CHECK_NEAR(result[0], ref, 1e-9);
            }
        }
    }

    #pragma omp target exit data map(release: x[0:N], y[0:N])
    #pragma omp target exit data map(release: result[0:1])

    free(x);
    free(y);
    free(result);

    TEST_PASS();
    return 0;
}
