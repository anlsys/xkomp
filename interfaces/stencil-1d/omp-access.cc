# include <assert.h>
# include <omp.h>
# include <stdlib.h>
# include <stdio.h>

# define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
# define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

# pragma omp begin declare target

static void
stencil(float * dst, float * src, size_t x, size_t y, int i, size_t chunk_size)
{
    // TODO
}

# pragma omp end declare target

int
main(void)
{
    int          niter = 10;
    int       ndevices = omp_get_num_devices();
    size_t        size = 1024;
    size_t       ghost = 1;
    float    * domain1 = (float *) malloc(sizeof(float) * size);
    float    * domain2 = (float *) malloc(sizeof(float) * size);
    size_t chunk_size = size / ndevices;
    assert(domain1 && domain2);
    assert(size % chunk_size == 0);

    for (int i = 0 ; i < size ; ++i)
    {
        domain1[i] = 1.0f;
        domain2[i] = 1.0f;
    }

    # pragma omp parallel
    {
        # pragma omp single
        {
            for (int iter = 0 ; iter < niter ; ++iter)
            {
                float * d1 = (iter % 2 == 0) ? domain1 : domain2;
                float * d2 = (iter % 2 == 0) ? domain2 : domain1;

                for (int i = 0 ; i < ndevices ; ++i)
                {
                    size_t a1 = (i+0)*chunk_size;
                    size_t b1 = (i+1)*chunk_size;                 // a1       b1
                    size_t a2 = MAX(0,    a1 - ghost);            // a2           b2
                    size_t b2 = MIN(size, b1 + ghost);            //  x x x x x    x . . . .    . . . . .    . . . . . 
                    # pragma omp target nowait device(i)    \
                        access(write: segment(d1, a1, b1))  \
                        access(read:  segment(d2, a2, b2))  \
                        stencil(d1, d2, a1, b1, i, chunk_size);
                }
            }

            # pragma omp target nowait device(omp_get_initial_device()) access(read: segment(domain1, 0, size)
                puts("Domain is now coherent on the host");

            # pragma omp taskwait

            // TODO: need some pragma to release device replicas
            # pragma omp target exit data map(storage: segment(domain, 0, size))

        }   /* single */
    } /* parallel */

    return 0;
}
