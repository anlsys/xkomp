# include <assert.h>
# include <stdio.h>
# include <stdlib.h>

# include <omp.h>

# define N 1024

//
//  This is a simplified version, that only executes taskgraph
//      - with its implicit taskgroup
//      - the same single thread creates and replay the taskgraph record
//

# include <xkomp/xkomp.h>

XKRT_NAMESPACE_USE;

static inline void
pragma_omp_taskgraph(std::function<void(void)> f)
{
    constexpr xkomp_taskgraph_id_t graph_id = 0;
    constexpr xkomp_taskgraph_flags_t flags = (xkomp_taskgraph_flags_t) 0;
    xkomp_taskgraph_t * taskgraph = xkomp_taskgraph_begin(graph_id, flags);
    f();
    xkomp_taskgraph_end(taskgraph);
}

int
main(void)
{
    char * deps = (char *) 0x1000;

    double * x = (double *) calloc(1, sizeof(double) * N);
    assert(x);

    # pragma omp parallel num_threads(2)
    {
        # pragma omp single
        {
            //////////////////
            // OpenMP setup //
            //////////////////

            for (int omp_device_num = 0 ; omp_device_num < omp_get_num_devices() ; ++omp_device_num)
            {
                # pragma omp target enter data map(alloc: x[0:N]) device(omp_device_num)
            }

            for (int iter = 0 ; iter < 2 ; ++iter)
            {
                pragma_omp_taskgraph([&] (void)
                {
                    for (int omp_device_num = 0 ; omp_device_num < omp_get_num_devices() ; ++omp_device_num)
                    {
                        # pragma omp target update to(x[0:N]) device(omp_device_num) depend(out: deps[omp_device_num]) nowait

                        for (int i = 0 ; i < 4 ; ++i)
                        {
                            # pragma omp target teams distribute parallel for device(omp_device_num) depend(inoutset: deps[omp_device_num]) nowait
                            for (int i = 0 ; i < N ; ++i)
                                ;
                        }

                        # pragma omp target update from(x[0:N]) device(omp_device_num) nowait depend(in: deps[omp_device_num])

                        # pragma omp task depend(out: deps[omp_device_num])
                            printf("Device %d completed iter %d\n", omp_device_num, iter);
                    }
                });
            }

            for (int omp_device_num = 0 ; omp_device_num < omp_get_num_devices() ; ++omp_device_num)
            {
                # pragma omp target exit data map(release: x[0:N]) device(omp_device_num)
            }
        }
    }
    return 0;
}
