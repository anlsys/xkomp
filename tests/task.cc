# include <assert.h>
# include <stdio.h>
# include <unistd.h>

int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single
        {
            int x = 42;
            int y = 43;
            int z = 44;

            # pragma omp task shared(x, z) firstprivate(y)
            {
                puts("Hello world");
                assert(x == 42);
                assert(y == 43);
                assert(z == 44);
            }

            # pragma omp task shared(x) firstprivate(y) untied
            {
                puts("Hello world !!");
                assert(x == 42);
                assert(y == 43);
            }

            # pragma omp taskwait
        }
    }

    return 0;
}
