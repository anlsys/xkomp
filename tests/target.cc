# include <assert.h>
# include <stdio.h>
# include <stdlib.h>

# include <omp.h>

# define N 1024

# define DEVICE_ID 0

int
main(void)
{
    double * x = (double *) malloc(sizeof(double) * N);
    assert(x);
    for (int i = 0 ; i < N ; ++i)
    {
        x[i] = i;
    }

    # pragma omp parallel num_threads(2)
    {
        /* Init */
        # pragma omp single
        {
            # pragma omp target enter data map(alloc: x[0:N]) device(DEVICE_ID)
        }

        /* Synchronous version */
        # pragma omp single
        {
            # pragma omp target update to(x[0:N]) device(DEVICE_ID)

            # pragma omp target teams distribute parallel for device(DEVICE_ID)
            for (int i = 0 ; i < N ; ++i)
                x[i] = x[i] * 2;

            # pragma omp target update from(x[0:N]) device(DEVICE_ID)

            // test: ensure the vector is correct on host
            for (int i = 0 ; i < N ; ++i)
            {
                assert(x[i] == i * 2);
            }
        }

        /* Asynchronous version */
        # pragma omp single
        {
            # pragma omp target update to(x[0:N]) device(DEVICE_ID) depend(out: x) nowait

            # pragma omp target teams distribute parallel for device(DEVICE_ID) depend(out: x) nowait
            for (int i = 0 ; i < N ; ++i)
                x[i] = x[i] * 2;

            # pragma omp target update from(x[0:N]) device(DEVICE_ID) nowait depend(in: x)

            # pragma omp taskwait

            // test: ensure the vector is correct on host
            for (int i = 0 ; i < N ; ++i)
            {
                assert(x[i] == i * 2 * 2);
            }
        }

        /* Deinit */
        # pragma omp single
        {
            # pragma omp target exit data map(release: x[0:N]) device(DEVICE_ID)
        }
    }
    return 0;
}
