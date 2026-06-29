// xkomp: supported  (depend(inoutset:) -> concurrent writers on a token)
//
// Pattern:  producer (out)  ->  N inoutset tasks (run after producer, mutually
// concurrent)  ->  consumer (in, runs after the whole inoutset set).
// The consumer must observe every inoutset task as completed.

#include "common.h"

#define M 8

static void
test_inoutset(void)
{
    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            int x = 0;            // dependency token
            int produced = 0;     // set by the producer
            int set_count = 0;    // bumped by each inoutset task
            int consumer_saw = -1;

            // producer
            #pragma omp task depend(out: x) shared(produced)
            {
                produced = 1;
            }

            // inoutset set: each runs after the producer; siblings may overlap
            for (int i = 0; i < M; ++i)
            {
                #pragma omp task depend(inoutset: x) firstprivate(i) shared(produced, set_count)
                {
                    CHECK_EQ(produced, 1);   // ordered after producer
                    #pragma omp atomic
                    set_count += 1;          // concurrent -> atomic
                }
            }

            // consumer: must run after the entire inoutset set
            #pragma omp task depend(in: x) shared(set_count, consumer_saw)
            {
                consumer_saw = set_count;
            }

            #pragma omp taskwait
            CHECK_EQ(set_count, M);
            CHECK_EQ(consumer_saw, M);
        }
    }
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_inoutset();

    TEST_PASS();
    return 0;
}
