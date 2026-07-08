// xkomp: supported  (parallel + single + task + depend + taskwait)
//
// A 3-stage per-item pipeline expressed as a task DAG:
//     gen(out:a) -> transform(in:a,out:b) -> finalize(in:b,out:c)
// Stages of the same item are ordered by handle-based dependences; different
// items are independent and may run concurrently.

#include "common.h"

#define ITEMS 16

static void
test_pipeline(void)
{
    int a[ITEMS], b[ITEMS], c[ITEMS];
    memset(a, 0, sizeof(a));
    memset(b, 0, sizeof(b));
    memset(c, 0, sizeof(c));

    #pragma omp parallel num_threads(4) shared(a, b, c)
    {
        #pragma omp single
        {
            for (int s = 0; s < ITEMS; ++s)
            {
                #pragma omp task depend(out: a[s]) firstprivate(s) shared(a) default(none)
                a[s] = s;

                #pragma omp task depend(in: a[s]) depend(out: b[s]) firstprivate(s) shared(a, b) default(none)
                {
                    CHECK_EQ(a[s], s);
                    b[s] = a[s] * 2;
                }

                #pragma omp task depend(in: b[s]) depend(out: c[s]) firstprivate(s) shared(b, c) default(none)
                {
                    CHECK_EQ(b[s], s * 2);
                    c[s] = b[s] + 1;
                }
            }
            #pragma omp taskwait
        }
    }

    for (int s = 0; s < ITEMS; ++s)
        CHECK_EQ(c[s], s * 2 + 1);
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_pipeline();

    TEST_PASS();
    return 0;
}
