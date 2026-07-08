// xkomp: supported  (XKOMP 'access' clause -- REQUIRES the patched LLVM/clang)
//
// The access clause declares which host storage a task reads/writes; the runtime
// derives region-based (interval) dependences from it.  These host tests pin the
// SYNCHRONIZATION semantics from the IWOMP'26 access-clause paper (Section 3.2)
// without needing a GPU: on the host the elected-replica is the initial-replica,
// so only task ordering is observable.
//
// Antecedent rule: Tu precedes Tv when their storage intersects (Su n Sv != 0)
// and  Mu=read & Mv=write,  or  Mu=write & Mv in {read,write}.
// read -> read does NOT create a dependence.
//
// (Mirrors iwomp26/access-examples/host.cc.)

#include "common.h"

#define N 64

static void
test_access_basic(void)
{
    static int x[N];
    memset(x, 0, sizeof(x));

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            // producer: writes the whole region
            #pragma omp task access(write: x[0:N]) default(none)
            {
                for (int i = 0 ; i < N ; ++i)
                    x[i] = i;
            }

            // consumer: reads one element -> ordered after the producer
            // (write -> read on intersecting storage)
            #pragma omp task access(read: x[N/2]) default(none)
            {
                CHECK_EQ(x[N/2], N/2);
            }

            // read+write over the whole region, non-coherent (synchronize only,
            // no coherence copy) -> ordered after both tasks above
            #pragma omp task access(noncoherent, read, write: x[0:N]) default(none)
            {
                CHECK_EQ(x[0], 0);
                x[0] = 123;
            }

            #pragma omp taskwait
            CHECK_EQ(x[0],   123);
            CHECK_EQ(x[N/2], N/2);
        }
    }
}

int
main(void)
{
    for (int r = 0 ; r < REPETITIONS ; ++r)
        test_access_basic();

    TEST_PASS();
    return 0;
}
