// xkomp: supported  (fan-out / fan-in task DAG via in/out dependences)
//
//            root (out:src)
//          /    |     |    \
//      mid[0] mid[1] mid[2] mid[3]      (in:src, out:mid[k])
//          \    |     |    /
//             sink (in: mid[0..3])
//
// Tokens are distinct scalar addresses (handle-based deps).  The join lists all
// four middle tokens explicitly so it is ordered after every middle task.

#include "common.h"

#define W 4

static void
test_dag(void)
{
    int src = 0;
    int mid[W];
    memset(mid, 0, sizeof(mid));
    int mid_done = 0;
    int sink = -1;

    #pragma omp parallel num_threads(4) shared(src, mid, mid_done, sink)
    {
        #pragma omp single
        {
            // root
            #pragma omp task depend(out: src) shared(src)
            {
                src = 10;
            }

            // fan-out
            for (int k = 0; k < W; ++k)
            {
                #pragma omp task depend(in: src) depend(out: mid[k]) \
                        firstprivate(k) shared(src, mid, mid_done)
                {
                    CHECK_EQ(src, 10);
                    mid[k] = src + k;
                    #pragma omp atomic
                    mid_done += 1;
                }
            }

            // fan-in: depends on every middle token
            #pragma omp task depend(in: mid[0], mid[1], mid[2], mid[3]) \
                    shared(mid, mid_done, sink)
            {
                CHECK_EQ(mid_done, W);
                int s = 0;
                for (int k = 0; k < W; ++k)
                    s += mid[k];
                sink = s;
            }

            #pragma omp taskwait
        }
    }

    int expected = 0;
    for (int k = 0; k < W; ++k)
        expected += 10 + k;   // src == 10
    CHECK_EQ(sink, expected);
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_dag();

    TEST_PASS();
    return 0;
}
