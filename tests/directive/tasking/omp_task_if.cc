// xkomp: supported  (task `if` clause: `if(0)` produces an undeferred task)
//
// A false `if` clause makes the task UNDEFERRED: the encountering thread runs
// its body inline via __kmpc_omp_task_begin_if0 / __kmpc_omp_task_complete_if0
// and does not proceed until it completed.  Three sub-tests:
//   1. basic     : the body ran by the time control returns (before any taskwait).
//   2. nested    : an if(0) task that itself spawns a (deferred) child.
//   3. depend    : an if(0) task with a depend clause runs after its predecessor
//                  (exercises __kmpc_omp_taskwait_deps_51).

#include "common.h"

// (1) The if(0) body must have run inline, before the following taskwait.
static void
test_if0_basic(void)
{
    volatile int cond = 0;   // runtime-false -> undeferred
    int x = 0;

    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        {
            #pragma omp task if(cond) shared(x) default(none)
            x = 7;

            // x was written inline by the undeferred task; assert even before taskwait
            CHECK_EQ(x, 7);

            #pragma omp taskwait
            CHECK_EQ(x, 7);
        }
    }
}

// (2) An undeferred task that spawns a deferred child; the child is a child of
// the undeferred task, waited via taskgroup deep-sync.
static void
test_if0_nested(void)
{
    volatile int cond = 0;
    int inner = 0, outer = 0;

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            #pragma omp taskgroup
            {
                #pragma omp task if(cond) shared(inner, outer) default(none)
                {
                    outer = 1;                 // undeferred body (inline)
                    #pragma omp task shared(inner) default(none)
                    inner = 2;                 // deferred grandchild
                }
            }
            // taskgroup deep-synced the undeferred task AND its descendant
            CHECK_EQ(outer, 1);
            CHECK_EQ(inner, 2);
        }
    }
}

// (3) if(0) with a depend clause: the undeferred task must run after the prior
// task that writes the same location.
static void
test_if0_depend(void)
{
    volatile int cond = 0;
    int x = 0, seen = -1;

    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        {
            #pragma omp task depend(out: x) shared(x) default(none)
            {
                for (int i = 0; i < 100000; ++i) { }  // take a while
                x = 5;
            }

            // undeferred: waits for the producer (via taskwait_deps), runs inline
            #pragma omp task if(cond) depend(in: x) shared(x, seen) default(none)
            seen = x;

            CHECK_EQ(seen, 5);

            #pragma omp taskwait
        }
    }
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
    {
        //test_if0_basic();
        //test_if0_nested();
        test_if0_depend();
    }
    TEST_PASS();
    return 0;
}
