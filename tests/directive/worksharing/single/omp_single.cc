// xkomp: supported  (__kmpc_single / __kmpc_end_single)
//
// Exactly one thread of the team executes the single region; the implicit
// barrier at its end makes the result visible to every thread.

#include "common.h"

static void
test_single(void)
{
    int entered = 0;       // how many threads entered the single
    int who = -1;          // which thread executed it

    #pragma omp parallel num_threads(4) shared(entered, who)
    {
        #pragma omp single
        {
            #pragma omp atomic
            entered += 1;
            who = omp_get_thread_num();
        }
        // implicit barrier here: every thread now observes the final values
        CHECK_EQ(entered, 1);
        CHECK(who >= 0);
        CHECK(who < omp_get_num_threads());
    }

    CHECK_EQ(entered, 1);
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_single();

    TEST_PASS();
    return 0;
}
