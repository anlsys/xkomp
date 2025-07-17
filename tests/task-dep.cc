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

            # pragma omp task depend(out: x)
                puts("Hello");

            # pragma omp task depend(in: x) depend(out: y)
                puts("world");


            # pragma omp task depend(in: y)
                puts("!");

            # pragma omp taskwait
        }
    }

    return 0;
}
