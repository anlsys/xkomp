# include <assert.h>
# include <omp.h>
# include <stdio.h>

int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single
        {
            printf("Thread %d out of %d threads\n", omp_get_thread_num(), omp_get_num_threads());

            int x = 0;
            int y = 0;

            // testing in/out
            # pragma omp task depend(out: x) shared(x, y)
            {
                assert(x == 0);
                x = 1;
            }

            # pragma omp task depend(in: x) depend(out: y) shared(x, y)
            {
                assert(x == 1);
                assert(y == 0);
                y = 1;
            }

            # pragma omp task depend(in: y) shared(x, y)
            {
                assert(x == 1);
                assert(y == 1);
            }

            # pragma omp taskwait
            assert(x == 1);
            assert(y == 1);

            // testing inoutset
            # pragma omp task depend(in: x) shared(x)
            {
                assert(x == 1);
                x = 2;
            }

            # pragma omp task depend(inoutset: x) shared(x)
            {
                assert(x == 2);
                x = 3;
            }

            # pragma omp task depend(in: x) shared(x)
            {
                assert(x == 3);
                x = 4;
            }

            # pragma omp task depend(inoutset: x) shared(x)
            {
                assert(x == 4);
                x = 5;
            }

            # pragma omp task depend(out: x) shared(x)
            {
                assert(x == 5);
                x = 6;
            }

            # pragma omp taskwait
            assert(x == 6);
            assert(y == 1);
        }
    }

    return 0;
}
