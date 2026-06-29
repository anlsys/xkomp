// xkomp: supported  (nested parallelism + nested worksharing + atomic)
//
// Outer `parallel for` distributes rows across a real team.  Inside each row a
// nested `parallel` region (serialized to one thread by XKOMP) runs an inner
// worksharing loop over the columns.  Correct results require the outer team,
// the nested region and the inner worksharing loop to all behave properly.

#include "common.h"

#define ROWS 32
#define COLS 64

static void
test_nested_worksharing(void)
{
    static long row_sum[ROWS];
    memset(row_sum, 0, sizeof(row_sum));

    #pragma omp parallel for num_threads(4) shared(row_sum)
    for (int r = 0; r < ROWS; ++r)
    {
        long s = 0;

        // nested region (one-thread team) with an inner worksharing loop
        #pragma omp parallel num_threads(3) shared(s) firstprivate(r)
        {
            #pragma omp for
            for (int c = 0; c < COLS; ++c)
            {
                #pragma omp atomic
                s += (long) (r + c);
            }
        }

        row_sum[r] = s;
    }

    for (int r = 0; r < ROWS; ++r)
    {
        long expected = 0;
        for (int c = 0; c < COLS; ++c)
            expected += (long) (r + c);
        CHECK_EQ(row_sum[r], expected);
    }
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_nested_worksharing();

    TEST_PASS();
    return 0;
}
