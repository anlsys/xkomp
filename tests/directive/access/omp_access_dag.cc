// xkomp: supported  (XKOMP 'access' clause -- REQUIRES the patched LLVM/clang)
//
// A host one-step 1D stencil expressed as a task DAG, mirroring the paper's
// flagship pattern (iwomp26/access-examples/stencil-1d) but on the host so it is
// deterministic and GPU-free.  Each tile reads its block plus a one-cell halo
// (overlapping the producer's region) and writes its output block.  The halo
// reads create the producer -> tile dependences via region intersection; the
// tiles are mutually independent and the final reader waits for all of them.

#include "common.h"

#define NT    4              // number of tiles
#define BS    16             // block size
#define N     (NT * BS)      // domain size
#define GHOST 1

static double dag_mem[N + 2 * GHOST];   // domain with one ghost cell on each side
static double dag_out[N];

static void
test_stencil_dag(void)
{
    double * u = dag_mem + GHOST;   // logical domain: indices [-GHOST, N+GHOST)
    memset(dag_out, 0, sizeof(dag_out));

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            // producer: initialise the whole padded domain (u[i] = i)
            #pragma omp task access(write: u[-GHOST : N + 2*GHOST])
            {
                for (int i = -GHOST ; i < N + GHOST ; ++i)
                    u[i] = (double) i;
            }

            // per-tile stencil: read [t*BS-GHOST, (t+1)*BS+GHOST) (halo overlaps
            // the producer) and write out[t*BS : BS]
            for (int t = 0 ; t < NT ; ++t)
            {
                #pragma omp task firstprivate(t)                       \
                        access(read:  u[t*BS - GHOST : BS + 2*GHOST])  \
                        access(write: dag_out[t*BS : BS])
                {
                    for (int k = t * BS ; k < (t + 1) * BS ; ++k)
                        dag_out[k] = u[k-1] + u[k] + u[k+1];
                }
            }

            // consumer: reads the whole output -> after every tile
            #pragma omp task access(read: dag_out[0:N])
            {
                for (int k = 0 ; k < N ; ++k)
                    CHECK_NEAR(dag_out[k], 3.0 * k, 1e-9);   // (k-1) + k + (k+1)
            }

            #pragma omp taskwait
            for (int k = 0 ; k < N ; ++k)
                CHECK_NEAR(dag_out[k], 3.0 * k, 1e-9);
        }
    }
}

int
main(void)
{
    for (int r = 0 ; r < REPETITIONS ; ++r)
        test_stencil_dag();

    TEST_PASS();
    return 0;
}
