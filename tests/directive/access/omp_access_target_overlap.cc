// xkomp: supported  (access clause: partially overlapping host/device regions)
//
// NOTE: needs the offload runtime (libomptarget); gated behind both
// XKOMP_TESTS_ACCESS and XKOMP_TESTS_TARGET.
//
// A device task writes one region; a host task then reads a *partially
// overlapping* region.  This exercises (a) the cross host/device dependence over
// intersecting-but-not-identical storage (impossible with depend), and (b) the
// partial copy-back: only the overlapping sub-region must be fetched from the
// device, while the non-overlapping part stays coherent on the host.
//
//   init (host):   x[i] = i                       for i in [0, N)
//   device write:  x[i] = 2*i  (region [0, 3N/4))
//   host read:     region [N/4, N)   -- overlaps the device write on [N/4, 3N/4)
//
//   => on [N/4, 3N/4) the host must observe the device's 2*i (copied back)
//      on [3N/4, N)   the host stayed coherent, so it still observes i

#include "common.h"

#define N       64
#define DEVICE  0

static int x[N];

int
main(void)
{
    for (int i = 0 ; i < N ; ++i)
        x[i] = i;                          // host is coherent for x[0:N]

    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        {
            // device doubles the sub-region [0, 3N/4)
            #pragma omp target teams distribute parallel for device(DEVICE) nowait \
                    access(read, write: x[0 : 3*N/4])
            for (int i = 0 ; i < 3*N/4 ; ++i)
                x[i] = 2 * x[i];

            // host reads [N/4, N): partially overlaps the device write on
            // [N/4, 3N/4); [3N/4, N) was never sent to the device
            #pragma omp task access(read: x[N/4 : 3*N/4])
            {
                for (int i = N/4   ; i < 3*N/4 ; ++i)
                    CHECK_EQ(x[i], 2 * i);     // fetched back from the device
                for (int i = 3*N/4 ; i < N     ; ++i)
                    CHECK_EQ(x[i], i);         // host stayed coherent
            }

            #pragma omp taskwait
            // the host read task above made the host coherent over [N/4, N)
            for (int i = N/4   ; i < 3*N/4 ; ++i)
                CHECK_EQ(x[i], 2 * i);
            for (int i = 3*N/4 ; i < N     ; ++i)
                CHECK_EQ(x[i], i);
            // note: x[0 : N/4) was written on the device but never read back,
            // so the host copy is intentionally left stale and not checked here.
        }
    }

    TEST_PASS();
    return 0;
}
