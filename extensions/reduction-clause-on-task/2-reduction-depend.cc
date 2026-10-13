# include <assert.h>

# define N 100

int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single
        {
            int x = 0;

            for (int i = 0 ; i < N ; ++i)
            {
                # pragma omp task reduction(+:x)
                    x += 1;
            }

            # pragma omp task depend(in: x)
            {
                assert(x == N);
            }

            # pragma omp taskwait

            assert(x == N);
        }
    }
    return 0;
}
