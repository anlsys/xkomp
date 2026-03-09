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
    unsigned char * x = (unsigned char *) calloc(1, sizeof(unsigned char) * size);
    assert(x);
    # if 0
    for (int i = 0 ; i < size ; ++i)
        x[i] = i % 256;
    # endif

    # pragma omp parallel num_threads(2)
    {
        # pragma omp single
        {
            //////////////////
            // OpenMP setup //
            //////////////////

            for (int omp_device_num = 0 ; omp_device_num < omp_get_num_devices() ; ++omp_device_num)
            {
                # pragma omp target enter data map(alloc: x[0:size]) device(omp_device_num)
            }

            printf("Number of devices: %d\n", omp_get_num_devices());
            int iter;
            for (iter = 0 ; iter < 5 ; ++iter)
            {
                double t0 = omp_get_wtime();
                constexpr xkomp_taskgraph_id_t graph_id = 0;
                constexpr xkomp_taskgraph_flags_t flags = XKOMP_TASKGRAPH_FLAG_NONE;
                pragma_omp_taskgraph(graph_id, flags, [&] (void)
                {
                    for (int omp_device_num = 0 ; omp_device_num < omp_get_num_devices() ; ++omp_device_num)
                    {
                        # pragma omp target update to(x[0:size]) device(omp_device_num) depend(out: deps[omp_device_num]) nowait

                        for (int j = 0 ; j < 3 ; ++j)
                        {
                            # pragma omp target teams distribute parallel for device(omp_device_num) depend(inoutset: deps[omp_device_num]) nowait
                            for (int i = 0 ; i < size ; ++i)
                                ; // x[i] = (2*i % 256);
                        }

                        # pragma omp target update from(x[0:size]) device(omp_device_num) nowait depend(in: deps[omp_device_num])

                        # pragma omp task depend(in: deps[omp_device_num]) firstprivate(omp_device_num, iter) default(none)
                            printf("Host task - Device %d completed iter %d\n", omp_device_num, iter);

                        # pragma omp task
                            printf("Host task independent completed\n");
                    }
                }
                );

                double tf = omp_get_wtime();
                printf("Iter %d took %lf us\n", iter, (tf - t0) * 1e6);
            }

            for (int omp_device_num = 0 ; omp_device_num < omp_get_num_devices() ; ++omp_device_num)
            {
                # pragma omp target exit data map(release: x[0:size]) device(omp_device_num)
            }

            # if 0
            for (int i = 0 ; i < size ; ++i)
            {
                if (size < 16)
                    printf("x[%d] = %d\n", i, x[i]);
                assert(x[i] == (2 * i % 256));
            }
            # endif

        }
    }
    return 0;
}
