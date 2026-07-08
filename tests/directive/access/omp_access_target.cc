// xkomp: supported  (XKOMP 'access' clause on target -- REQUIRES patched LLVM)
//
// NOTE: needs the offload runtime (libomptarget); gated behind both
// XKOMP_TESTS_ACCESS and XKOMP_TESTS_TARGET.
//
// The access clause on a `target nowait` construct combines the dependence with
// lazy memory management: the runtime allocates the device replica, copies in/out
// only as needed, and orders the host reader after the device writer.
//
// (Mirrors iwomp26/access-examples/target.cc.)

#include "common.h"

#define N       64
#define DEVICE  0

static int x[N];

int
main(void)
{
    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        {
            // device task writes the array (replica allocated + made coherent lazily)
            #pragma omp target teams distribute parallel for device(DEVICE) nowait \
                    access(write: x[0:N])
            for (int i = 0 ; i < N ; ++i)
                x[i] = i;

            // host task reads the whole array -> ordered after the device writer
            // (write -> read); the runtime copies the region back to the host
            #pragma omp task access(read: x[0:N]) default(none)
            {
                for (int i = 0 ; i < N ; ++i)
                    CHECK_EQ(x[i], i);
            }

            #pragma omp taskwait
            // the host read task above made the host replica coherent for x[0:N]
            for (int i = 0 ; i < N ; ++i)
                CHECK_EQ(x[i], i);
        }
    }

    TEST_PASS();
    return 0;
}
