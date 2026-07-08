// xkomp: supported  (__kmpc_omp_task_alloc / __kmpc_omp_task / __kmpc_omp_taskwait)
//
// A single thread spawns many tasks; the team executes them; taskwait joins.
// Also checks firstprivate (captured by value) vs shared (written back).

#include "common.h"

static void
test_many_tasks(void)
{
    static int done[NUM_TASKS];
    memset(done, 0, sizeof(done));
    int executed = 0;

    #pragma omp parallel num_threads(4) shared(done, executed)
    {
        #pragma omp single
        {
            for (int i = 0; i < NUM_TASKS; ++i)
            {
                #pragma omp task firstprivate(i) shared(done, executed) default(none)
                {
                    done[i] = 1;
                    #pragma omp atomic
                    executed += 1;
                }
            }
            #pragma omp taskwait

            for (int i = 0; i < NUM_TASKS; ++i)
                CHECK_EQ(done[i], 1);
            CHECK_EQ(executed, NUM_TASKS);
        }
    }
}

static void
test_data_sharing(void)
{
    int x = 42;   // firstprivate -> captured by value
    int y = 0;    // shared       -> written by the task

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            #pragma omp task firstprivate(x) shared(y) default(none)
            {
                CHECK_EQ(x, 42);
                y = x + 1;
            }
            #pragma omp taskwait
            CHECK_EQ(y, 43);
        }
    }
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
    {
        test_many_tasks();
        test_data_sharing();
    }

    TEST_PASS();
    return 0;
}
