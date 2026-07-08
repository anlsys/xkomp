// xkomp: supported  (access clause: multi-GPU 1D stencil) -- patched LLVM + offload
//
// NOTE: needs the offload runtime (libomptarget); gated behind both
// XKOMP_TESTS_ACCESS and XKOMP_TESTS_TARGET.
//
// Iterative 1D 3-point stencil distributed across all available devices, using
// the access clause for both the cross-device synchronization AND the lazy halo
// exchange (this is the paper's flagship example,
// iwomp26/access-examples/stencil-1d/main-access.cc).
//
// Each device j computes the interior block [j*bs, (j+1)*bs) into the output
// buffer, reading [j*bs-GHOST, (j+1)*bs+GHOST) of the input buffer.  The +/-GHOST
// halo cells overlap the *neighbouring* devices' write regions, so the access
// clause derives the cross-device dependences and copies only the ghost cells
// between devices each iteration.
//
// Correctness is checked against an independent serial host computation using
// the exact same schedule and a non-trivial (diffusing) field, so a stale or
// missing halo exchange would diverge from the reference.

#include "common.h"

#include <stdlib.h>

#define GHOST   1
#define BS      32
#define N_ITER  8

typedef double TYPE;

// bounded, non-linear field so the diffusion actually changes values over
// iterations (a linear field would be a fixed point and hide halo bugs)
static TYPE
init_value(int i)
{
    int m = (i * 7 + 3) % 17;
    return (TYPE) (m - 8);
}

static void
stencil_step(TYPE * dst, const TYPE * src, int n)
{
    for (int k = 0 ; k < n ; ++k)
    {
        TYPE acc = 0;
        for (int g = -GHOST ; g <= GHOST ; ++g)
            acc += src[k + g];
        dst[k] = acc / (TYPE) (2 * GHOST + 1);
    }
}

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

    const int bs = BS;
    const int n  = BS * ndevices;          // divides evenly across devices
    const size_t span = (size_t) (n + 2 * GHOST);

    // two domains in a single allocation (as in the paper example)
    TYPE * mem = (TYPE *) malloc(2 * span * sizeof(TYPE));
    CHECK(mem != NULL);
    TYPE * u = mem + GHOST;
    TYPE * v = mem + span + GHOST;

    // independent host-reference buffers
    TYPE * refmem = (TYPE *) malloc(2 * span * sizeof(TYPE));
    CHECK(refmem != NULL);
    TYPE * ru = refmem + GHOST;
    TYPE * rv = refmem + span + GHOST;

    for (int i = -GHOST ; i < n + GHOST ; ++i)
    {
        u[i]  = v[i]  = init_value(i);
        ru[i] = rv[i] = init_value(i);     // ghosts act as fixed boundary values
    }

    // host reference: same double-buffered schedule, serial
    for (int it = 0 ; it < N_ITER ; ++it)
    {
        TYPE * tmp = ru; ru = rv; rv = tmp;
        stencil_step(rv, ru, n);
    }

    // GPU version: each iteration is distributed across the devices
    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        {
            for (int it = 0 ; it < N_ITER ; ++it)
            {
                TYPE * tmp = u; u = v; v = tmp;     // double buffering
                for (int j = 0 ; j < ndevices ; ++j)
                {
                    #pragma omp target teams distribute parallel for nowait device(j) \
                            access(read:  u[j*bs - GHOST : bs + 2*GHOST])             \
                            access(write: v[j*bs : bs])
                    for (int k = j*bs ; k < (j+1)*bs ; ++k)
                    {
                        TYPE acc = 0;
                        for (int g = -GHOST ; g <= GHOST ; ++g)
                            acc += u[k + g];
                        v[k] = acc / (TYPE) (2 * GHOST + 1);
                    }
                }
            }

            // write the final buffer back to the host initial-replica
            #pragma omp task access(read: v[0:n]) default(none)
                {}

            #pragma omp taskwait
        }
    }

    // the final GPU buffer (v) must match the host reference (rv): same number of
    // swaps from the same start, so both name the last-written buffer
    for (int i = 0 ; i < n ; ++i)
        CHECK_NEAR(v[i], rv[i], 1e-9);

    free(mem);
    free(refmem);

    TEST_PASS();
    return 0;
}
