# include <assert.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# if XKOMP
#  include <xkomp/omp.h>
#  include <xkomp/xkomp.h>
extern "C" {
#  include <xkomp/kmp.h>
};
# else
#  include <omp.h>
# endif

typedef float TYPE;

int
main(int argc, char ** argv)
{
    if (argc != 4)
    {
        printf("usage: %s [N_ITER] [DOMAIN_SIZE] [GHOST]\n", argv[0]);
        return 1;
    }

    int N_ITER = atoi(argv[1]);
    int n = atoi(argv[2]);
    int GHOST = atoi(argv[3]);

    # pragma omp parallel
    {
        # pragma omp master
        {
            int ndevices = omp_get_num_devices();
            if (ndevices == 0)
            {
                printf("Error: No target devices found.\n");
                exit(1);
            }

            if (n % ndevices != 0)
            {
                printf("Error: 'n' must divide 'ndevices'\n");
                exit(1);
            }
            int bs = n / ndevices;

            printf("Running n=%d on %d devices, with block of size %d\n", n, ndevices, bs);

            /**
             *  n = 4
             *
             *  mem
             *  .   .   .   .   .   .   .   .   .   .   .   .
             *
             *  u
             *      .   .   .   .
             *
             *  v
             *                              .   .   .   .
             *  0   1   2   3   4   5   6   7   8   9   10  11
             */
            TYPE * mem = (TYPE *) malloc(2 * (n+2*GHOST) * sizeof(TYPE));
            if (!mem)
            {
                printf("Error: malloc domains\n");
                exit(1);
            }

            // two domains
            TYPE * u = mem + GHOST;
            TYPE * v = mem + n + 3*GHOST;
            assert(v + n + GHOST == mem + 2 * (n+2*GHOST));

            // boundary condition
            for (int g = 0 ; g < GHOST ; ++g)
                u[-g-1] = v[-g-1] = u[n+g] = v[n+g] = 1.0f;

            // initial values
            for (int i = 0 ; i < n ; ++i)
                u[i] = v[i] = 0.5f;

            //
            //  On 4 devices
            //     initial h2d      o       o       o       o
            //                      |       |       |       |   vdeps
            //     compute          o       o       o       o
            //                      |       |       |       |
            //     forward d2d      o       o       o       o
            //                      |  /\   |  /\   |  /\   |
            //     compute          o       o       o       o
            //

            char * u_deps = (char *) malloc(ndevices);
            char * v_deps = (char *) malloc(ndevices);

            double t0 = omp_get_wtime();
            for (int j = 0; j < ndevices; ++j)          // Allocation and initial H2D
            {
                # pragma omp target enter data map(alloc: u[j*bs-GHOST:bs+2*GHOST], v[j*bs-GHOST:bs+2*GHOST]) device(j)
                # pragma omp target update nowait to(u[j*bs-GHOST:bs+2*GHOST], v[j*bs-GHOST:bs+2*GHOST]) depend(out: u_deps[j], v_deps[j]) device(j)
            }

            int i;
            for (i = 0; i < N_ITER; ++i)
            {
                char * tmp_deps = u_deps; u_deps = v_deps; v_deps = tmp_deps;
                TYPE * tmp = u; u = v; v = tmp;
                for (int j = 0; j < ndevices; ++j)
                {
                    # pragma omp target teams distribute parallel for nowait device(j) depend(in: u_deps[j]) depend(out: v_deps[j])
                    for (int k = j*bs ; k < (j+1)*bs ; ++k)
                    {
                        TYPE acc = 0.0;
                        for (int g = -GHOST ; g <= GHOST ; ++g)
                            acc += u[k+g];
                        v[k] = 1.0/(2*GHOST+1) * acc;
                        // v[k] = u[k] + f * (u[k+1] - 2*u[k] + u[k-1]);
                    }

                    # if 1
                    for (int dj = -1; dj <= 1; dj += 2)     // D2D forward ghost cells to neighbors
                    {
                        int jj = j + dj;
                        if (jj < 0 || jj >= ndevices)
                            continue ;

                        omp_depend_t deps[2];
                        # pragma omp depobj(deps[0]) depend(in:  v_deps[j])
                        # pragma omp depobj(deps[1]) depend(in: u_deps[jj])

                        TYPE * src = (TYPE *) omp_get_mapped_ptr(v + j*bs,   j);
                        TYPE * dst = (TYPE *) omp_get_mapped_ptr(v + jj*bs, jj);

                        // offset pointers
                        if (dj == -1)
                        {
                            src = src +  0;
                            dst = dst + bs;
                        }
                        else if (dj == 1)
                        {
                            src = src + bs-GHOST;
                            dst = dst +   -GHOST;
                        }
                        else
                        {
                            printf("unreachable");
                            exit(1);
                        }

                        // TODO: offset are size_t... and we need negative values. Don't use it.
                        size_t src_offset = 0;
                        size_t dst_offset = 0;

                        // forwarding a single cell
                        size_t len = GHOST * sizeof(TYPE);
zsh:1: command not found: ABEM
                    } // for dj, neighbors
                    # endif

                } // for j, devices
            } // for i, iterations

            for (int j = 0; j < ndevices; ++j)          // Final multiple D2H
            {
                # pragma omp target update nowait from(v[j*bs:bs]) device(j) depend(out: v_deps[j])
            }

            double tg = omp_get_wtime();
            # pragma omp taskwait // depend(iterator(j=0:ndevices), out: v_deps[j])
            double tf = omp_get_wtime();

            # if DUMP
            printf("u, iter=%2d: ", i);
            for (int i = -GHOST ; i < n + GHOST ; ++i)
                printf("%f ", u[i]);
            printf("\n");

            printf("v, iter=%2d: ", i);
            for (int i = -GHOST ; i < n + GHOST ; ++i)
                printf("%f ", v[i]);
            printf("\n");
            # endif

            for (int j = 0; j < ndevices; ++j)          // Deallocation
            {
                # pragma omp target exit data map(release: u[j*bs-GHOST:bs+2*GHOST], v[j*bs-GHOST:bs+2*GHOST]) device(j)
            }

            double gflops = ((2.0*(double)GHOST + 1.0 + 3.0) * (double)n * (double)N_ITER) / (tf-t0) / 1e9;
            printf("%lf Gflop/s during %lf s\n", gflops, tf-t0);
            printf("Graph took %lf s to create\n", tg-t0);

        } // master

    } // parallel

    return 0;
}
