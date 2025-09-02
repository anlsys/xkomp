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
            # pragma omp target device(DEVICE_ID) access() nowait
            {
                printf("Running from device `%d` is initial: %d\n",
                        omp_get_device_num(), omp_is_initial_device());
                assert(omp_is_initial_device() == 0);
            }

            # pragma omp taskwait
        }
    }
    return 0;
}
