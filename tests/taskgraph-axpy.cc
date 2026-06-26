# include <assert.h>
# include <stdio.h>
# include <stdlib.h>
# include <time.h>
# include <unistd.h>

# include <omp.h>

//
//  This is a simplified version, that only executes taskgraph
//      - with its implicit taskgroup
//      - the same single thread creates and replay the taskgraph record
//

# define USE_TASKGRAPH 1

# if USE_TASKGRAPH
#  include <xkomp/xkomp.h>
#  include <xkomp/xkomp++.h>
XKRT_NAMESPACE_USE;
# endif

int
main(void)
{
    const int ndevices = omp_get_num_devices();
    assert(ndevices);

    constexpr size_t size  = 16;
    const     size_t bs    = size / ndevices;
    const     float  beta  = 0.3f;
    const     float  alpha = 0.7f;
    assert(size % bs == 0);

    printf("Running on %d devices with total size %zu of block size %zu\n", ndevices, size, bs);

    float * x = (float *) malloc(sizeof(float) * size);
    float * y = (float *) malloc(sizeof(float) * size);
    float * z = (float *) malloc(sizeof(float) * size);
    assert(x);
    assert(y);
    assert(z);
    for (int i = 0 ; i < size ; ++i)
    {
        x[i] = ((float) i) + 0.1f;
        y[i] = ((float) i) + 1.0f;
        z[i] = y[i];
    }

    # pragma omp parallel num_threads(2)
    {
        # pragma omp single
        {
            //////////////////
            // OpenMP setup //
            //////////////////

            for (int omp_device_num = 0 ; omp_device_num < ndevices ; ++omp_device_num)
            {
                # pragma omp target enter data map(alloc: x[omp_device_num*bs:bs]) device(omp_device_num)
                # pragma omp target enter data map(alloc: y[omp_device_num*bs:bs]) device(omp_device_num)
            }

            printf("Number of devices (excluding host): %d\n", ndevices);
            int iter;
            for (iter = 0 ; iter < 10 ; ++iter)
            {
                double t0 = omp_get_wtime();
                # if USE_TASKGRAPH
                constexpr xkomp_taskgraph_id_t graph_id = 0;
                constexpr xkomp_taskgraph_flags_t flags = XKOMP_TASKGRAPH_FLAG_NONE;
                pragma_omp_taskgraph(graph_id, flags, [&] (void)
                # endif
                {
                    for (int omp_device_num = 0 ; omp_device_num < ndevices ; ++omp_device_num)
                    {
                        int j = (omp_device_num+0)*bs;
                        // int k = (omp_device_num+1)*bs;
                        # pragma omp target update to(x[j:bs]) device(omp_device_num) depend(in: x) nowait
                        # pragma omp target update to(y[j:bs]) device(omp_device_num) depend(in: x) nowait

                        # pragma omp target teams distribute parallel for device(omp_device_num) nowait depend(out: x) firstprivate(beta)
                        for (int i = 0 ; i < bs ; ++i)
                            y[i+j] = beta * y[i+j];

                        # pragma omp target teams distribute parallel for device(omp_device_num) nowait depend(out: x) firstprivate(alpha)
                        for (int i = 0 ; i < bs ; ++i)
                            y[i+j] = alpha * x[i+j] + y[i+j];

                        # pragma omp target update from(y[j:bs]) device(omp_device_num) depend(out: x) nowait
                    }
                }
                # if USE_TASKGRAPH
                );
                # endif

                double tf = omp_get_wtime();
                printf("Iter %d took %lf us\n", iter, (tf - t0) * 1e6);
            }

            for (int omp_device_num = 0 ; omp_device_num < ndevices ; ++omp_device_num)
            {
                # pragma omp target exit data map(release: x[0:size]) device(omp_device_num)
                # pragma omp target exit data map(release: y[0:size]) device(omp_device_num)
            }

            # if 0
            for (int i = 0 ; i < size ; ++i)
            {
                // printf("%lf vs %lf\n", y[i], alpha * x[i] + z[i]);
                assert(abs(y[i] - (alpha * x[i] + z[i])) / y[i] * 100.0f < 0.1f);
            }
            assert(printf("Test passed\n"));
            # endif
        }
    }
    return 0;
}
