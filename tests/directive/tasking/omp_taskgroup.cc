// xkomp: supported  (taskgroup: deep synchronization of a task subtree)
//
// `taskgroup` waits at its end for all DESCENDANT tasks (child tasks and their
// descendants), unlike `taskwait` which only waits for direct children.  Two
// sub-tests:
//   1. flat        : the region's child tasks must all have run at end.
//   2. deep-sync   : each child spawns a GRANDCHILD but does NOT taskwait for it;
//                    only the taskgroup's deep sync guarantees the grandchildren
//                    have run once the region ends -- a plain taskwait would not.

#include "common.h"

// The region's child tasks must all have completed at the end of the taskgroup.
static void
test_taskgroup_flat(void)
{
    static int done[NUM_TASKS];
    memset(done, 0, sizeof(done));

    #pragma omp parallel num_threads(4) shared(done)
    {
        #pragma omp single
        {
            #pragma omp taskgroup
            {
                for (int i = 0; i < NUM_TASKS; ++i)
                {
                    #pragma omp task firstprivate(i) shared(done) default(none)
                    done[i] = 1;
                }
            }

            for (int i = 0; i < NUM_TASKS; ++i)
                CHECK_EQ(done[i], 1);
        }
    }
}

// Deep sync: every child task spawns a grandchild without waiting for it.  Only
// taskgroup's descendant-wide wait guarantees the grandchildren have run by the
// time the region ends (a plain taskwait waits for the children only).
static void
test_taskgroup_deep(void)
{
    static int grandchild[NUM_TASKS];
    memset(grandchild, 0, sizeof(grandchild));

    #pragma omp parallel num_threads(4) shared(grandchild)
    {
        #pragma omp single
        {
            #pragma omp taskgroup
            {
                for (int i = 0; i < NUM_TASKS; ++i)
                {
                    // child task: spawns a grandchild, then returns WITHOUT a
                    // taskwait, so the grandchild may still be pending when the
                    // child completes.
                    #pragma omp task firstprivate(i) shared(grandchild) default(none)
                    {
                        #pragma omp task firstprivate(i) shared(grandchild) default(none)
                        grandchild[i] = 1;
                    }
                }
            }

            // taskgroup end deep-synced the whole subtree: grandchildren are done.
            for (int i = 0; i < NUM_TASKS; ++i)
                CHECK_EQ(grandchild[i], 1);
        }
    }
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
    {
        test_taskgroup_flat();
        test_taskgroup_deep();
    }
    TEST_PASS();
    return 0;
}
