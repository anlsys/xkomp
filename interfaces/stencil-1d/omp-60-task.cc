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
            char virtual_deps[ndevices+2];  // ugly VLA, whatever

            for (int i = 0 ; i < ndevices ; ++i)
            {
                size_t x = MAX(0,    (i+0)*chunk_size - ghost);
                size_t y = MIN(size, (i+1)*chunk_size + ghost);

                # pragma omp target enter data map(storage: domain1[x:y-x], domain2[x:y-x]) device(i)

                # pragma omp target update nowait depend(out: virtual_deps[i+1]) to(domain1[x:y-x], domain2[x:y-x])
            }

            for (int iter = 0 ; iter < niter ; ++iter)
            {
                float * d1 = (iter % 2 == 0) ? domain1 : domain2;
                float * d2 = (iter % 2 == 0) ? domain2 : domain1;

                for (int i = 0 ; i < ndevices ; ++i)
                {
                    size_t ai = MAX(0,    (i+0)*chunk_size - ghost);
                    size_t bi = MIN(size, (i+1)*chunk_size + ghost);
                    
                    # pragma omp target nowait device(i)                \
                        depend(in:  virtual_deps[i+0])                  \
                        depend(out: virtual_deps[i+1])                  \
                        depend(in:  virtual_deps[i+2])
                        stencil(d1, d2, ai, bi, i, chunk_size);

                    // forward ghost cells to neighbors
                    for (int j = i - 1 ; j < i + 1 ; ++j)
                    {
                        if (i == j || j < 0 || j >= ndevices)
                            continue ;
                        
                        size_t aj = MAX(0,    (j+0)*chunk_size - ghost);
                        size_t bj = MIN(size, (j+1)*chunk_size + ghost);

                        # pragma omp task depend(out: virtual_deps[i+1])
                        {
                            void * src = omp_get_mapped_ptr(d1 + ai, i);
                            void * dst = omp_get_mapped_ptr(d2 + aj, j);
                            size_t len = ghost;
                            size_t src_offset = (j==i-1) ?                  0 : chunk_size + ghost; // forward 'left'
                            size_t dst_offset = (j==i-1) ? chunk_size + ghost :                  0; // forward 'right'
                            int src_device = i;
                            int dst_device = j;

                            // almost certain that we loose a thread here in some cuStreamSynchronize-alike calls
                            omp_target_memcpy(dst, src, len, dst_offset, src_offset, dst_device, src_device);
                        }
                    }
                }
            }

            // D2H
            for (int i = 0 ; i < ndevices ; ++i)
            {
                size_t x = MAX(0,    (i+0)*chunk_size - ghost);
                size_t y = MIN(size, (i+1)*chunk_size + ghost);
                # pragma omp target update nowait depend(out: virtual_deps[i+1]) device(i) from(domain1[x:y-x])
            }

            // depend on all previous D2H
            # pragma omp target nowait device(omp_get_initial_device()) depend(iterator(i=0:ndevices), in: virtual_deps[i+1])
                puts("Domain is now coherent on the host");

            # pragma omp taskwait

            for (int i = 0 ; i < ndevices ; ++i)
            {
                size_t x = MAX(0,    (i+0)*chunk_size - ghost);
                size_t y = MIN(size, (i+1)*chunk_size + ghost);
                // TODO: should be 'storage' rather than 'release' - but LLVM implements it as 'release'
                # pragma omp target exit data map(storage: domain1[x:y-x], domain2[x:y-x]) device(i)
            }

        }   /* single */
    } /* parallel */

    return 0;
}
