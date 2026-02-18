# include <assert.h>
# include <stdio.h>
# include <stdlib.h>

# include <omp.h>

# define N 1024

# define DEVICE_ID 0

int
main(void)
{
    double * x = (double *) calloc(1, sizeof(double) * N);
    assert(x);

    # pragma omp parallel num_threads(2)
    {
        # pragma omp single
        {
            # pragma omp target enter data map(alloc: x[0:N]) device(DEVICE_ID)

            # pragma omp target update to(x[0:N]) device(DEVICE_ID) depend(out: x) nowait

            # pragma omp target teams distribute parallel for device(DEVICE_ID) depend(out: x) nowait
            for (int i = 0 ; i < N ; ++i)
                x[i] = i;

            # if 0
            # pragma omp target device(DEVICE_ID) depend(out: x) nowait
            {
                printf("Running as a task from device `%d` is initial: %d\n",
                        omp_get_device_num(), omp_is_initial_device());
            }
            # endif

            # pragma omp target update from(x[0:N]) device(DEVICE_ID) nowait depend(in: x)

            # pragma omp taskwait

            # pragma omp target exit data map(release: x[0:N]) device(DEVICE_ID)


            // test: ensure the vector is correct on host
            for (int i = 0 ; i < N ; ++i)
            {
                assert(x[i] == i);
            }
        }
    }
    return 0;
}
