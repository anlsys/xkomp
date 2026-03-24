# include <assert.h>
# include <stdio.h>
# include <stdlib.h>

# include <omp.h>

//
//  This is a simplified version, that only executes taskgraph
//      - with its implicit taskgroup
//      - the same single thread creates and replay the taskgraph record
//

# include <xkomp/xkomp.h>
# include <xkomp/xkomp++.h>

XKRT_NAMESPACE_USE;

int
main(void)
{
    char * deps = (char *) 0x1000;

    // constexpr size_t size = 1024*1024*1024;
    constexpr size_t size = 1024;
    const     size_t bs   = size / omp_get_num_devices();
    const     float  alpha = 0.7f;

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

            for (int omp_device_num = 0 ; omp_device_num < omp_get_num_devices() ; ++omp_device_num)
            {
                # pragma omp target enter data map(alloc: x[omp_device_num*bs:bs]) device(omp_device_num)
                # pragma omp target enter data map(alloc: y[omp_device_num*bs:bs]) device(omp_device_num)
            }

            printf("Number of devices: %d\n", omp_get_num_devices());
            int iter;
            for (iter = 0 ; iter < 1 ; ++iter)
            {
                double t0 = omp_get_wtime();
                constexpr xkomp_taskgraph_id_t graph_id = 0;
                constexpr xkomp_taskgraph_flags_t flags = XKOMP_TASKGRAPH_FLAG_NONE;
                pragma_omp_taskgraph(graph_id, flags, [&] (void)
                {
                    # pragma omp task depend(out: x)
                        {}

                    for (int omp_device_num = 0 ; omp_device_num < omp_get_num_devices() ; ++omp_device_num)
                    {
                        # pragma omp target update to(x[omp_device_num*bs:bs]) device(omp_device_num) depend(in: x) depend(inoutset: deps[omp_device_num]) nowait
                        # pragma omp target update to(y[omp_device_num*bs:bs]) device(omp_device_num) depend(in: x) depend(inoutset: deps[omp_device_num]) nowait

                        # pragma omp target teams distribute parallel for device(omp_device_num) nowait \
                            depend(out: deps[omp_device_num])                                           \
                            firstprivate(alpha)
                        for (int i = omp_device_num*bs ; i < bs ; ++i)
                            y[i] = alpha * x[i] + y[i];

                        # pragma omp target update from(y[omp_device_num*bs:bs]) device(omp_device_num) depend(out: deps[omp_device_num]) nowait
                    }

                    # pragma omp task depend(in: x)
                        {}
                }
                );

                double tf = omp_get_wtime();
                printf("Iter %d took %lf us\n", iter, (tf - t0) * 1e6);
            }

            for (int omp_device_num = 0 ; omp_device_num < omp_get_num_devices() ; ++omp_device_num)
            {
                # pragma omp target exit data map(release: x[0:size]) device(omp_device_num)
                # pragma omp target exit data map(release: y[0:size]) device(omp_device_num)
            }

            for (int i = 0 ; i < size ; ++i)
                assert(abs(y[i] - (alpha * x[i] + z[i])) / y[i] * 100.0f < 0.1f);
        }
    }
    return 0;
}
