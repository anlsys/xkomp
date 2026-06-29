// xkomp: supported  (simple scalar atomics are lowered inline by clang; no
//        __kmpc_atomic_* runtime call is required for these forms)
//
// Exercises atomic update, capture, read and write under contention.

#include "common.h"

static void
test_atomic(void)
{
    int counter = 0;     // atomic update target
    int nth = 0;

    // 1) atomic update under contention
    #pragma omp parallel num_threads(8) shared(counter, nth)
    {
        if (omp_get_thread_num() == 0)
            nth = omp_get_num_threads();

        for (int k = 0; k < LOOPCOUNT; ++k)
        {
            #pragma omp atomic update
            counter += 1;
        }
    }
    CHECK_EQ(counter, nth * LOOPCOUNT);

    // 2) atomic capture: hand out unique tickets, then assert no duplicates
    int next = 0;
    static int ticket_seen[XKOMP_TEST_MAXT];
    memset(ticket_seen, 0, sizeof(ticket_seen));
    CHECK(nth <= XKOMP_TEST_MAXT);

    #pragma omp parallel num_threads(nth) shared(next, ticket_seen)
    {
        int my;
        #pragma omp atomic capture
        { my = next; next += 1; }

        CHECK(my >= 0);
        CHECK(my < nth);
        // each ticket value handed out exactly once
        #pragma omp atomic
        ticket_seen[my] += 1;
    }
    CHECK_EQ(next, nth);
    for (int i = 0; i < nth; ++i)
        CHECK_EQ(ticket_seen[i], 1);

    // 3) atomic read / write of a shared flag
    int flag = 0;
    #pragma omp parallel num_threads(nth) shared(flag)
    {
        if (omp_get_thread_num() == 0)
        {
            #pragma omp atomic write
            flag = 7;
        }
        #pragma omp barrier
        int v;
        #pragma omp atomic read
        v = flag;
        CHECK_EQ(v, 7);
    }
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_atomic();

    TEST_PASS();
    return 0;
}
