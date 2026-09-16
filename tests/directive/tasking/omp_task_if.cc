// xkomp: supported  (task `if` clause: `if(0)` produces an undeferred task)
//
// A false `if` clause makes the task UNDEFERRED. In XKOMP the task is still
// submitted on the normal scheduling path (any thread of the team may run it),
// but the encountering thread suspends until that task completed before
// proceeding -- conveyed by an `undeferred` bit in kmp_tasking_flags set at
// creation, and honoured after commit in __kmpc_omp_task[_with_deps_v2].
// Three sub-tests:
//   1. basic     : the task has completed by the time control returns (before any taskwait).
//   2. nested    : an if(0) task that itself spawns a (deferred) child.
//   3. depend    : an if(0) task with a depend clause runs after its predecessor.

#include "common.h"

// (1) The if(0) task must have completed by the time the construct returns.
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

            // the encountering thread suspended until the undeferred task finished,
            // so x is already written here -- even before any taskwait
            CHECK_EQ(x, 7);

            #pragma omp taskwait
            CHECK_EQ(x, 7);
        }
    }
}

// (2) An undeferred task that spawns a deferred child. The undeferred task is
// waited by the encountering thread; its descendant is waited by the taskgroup.
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
                    outer = 1;                 // undeferred task body
                    #pragma omp task shared(inner) default(none)
                    inner = 2;                 // deferred grandchild
                }
            }
            // the undeferred task completed (waited on spawn) AND the taskgroup
            // deep-synced its descendant
            CHECK_EQ(outer, 1);
            CHECK_EQ(inner, 2);
        }
    }
}

// (3) if(0) with a depend clause: the undeferred task carries its dependences and
// is committed on the normal path, so it runs after its predecessor; the
// encountering thread then suspends until it completed.
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

            // undeferred: ordered after the producer via its own depend clause
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
        test_if0_basic();
        test_if0_nested();
        test_if0_depend();
    }
    TEST_PASS();
    return 0;
}
