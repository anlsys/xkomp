# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>

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

            // boundary condition
            for (int g = 0 ; g < GHOST ; ++g)
                u[-g-1] = v[-g-1] = u[n+g] = v[n+g] = 1.0f;

            // initial values
            for (int i = 0 ; i < n ; ++i)
                u[i] = v[i] = 0.5f;

            double t0 = omp_get_wtime();

            for (int i = 0; i < N_ITER; ++i)
            {
                TYPE * tmp = u; u = v; v = tmp;
                for (int j = 0; j < ndevices; ++j)
                {
                    # pragma omp target teams distribute parallel for nowait device(j) access(read: u[j*bs-GHOST:bs+2*GHOST]) access(write: v[j*bs:bs])
                    for (int k = j*bs ; k < (j+1)*bs ; ++k)
                    {
                        TYPE acc = 0.0;
                        for (int g = -GHOST ; g <= GHOST ; ++g)
                            acc += u[k+g];
                        v[k] = 1.0/(2*GHOST+1) * acc;
                    }

                } // for j, devices
            } // for i, iterations

            double tg = omp_get_wtime();
            # pragma omp taskwait access(read: v[0:n])
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
            double gflops = ((2.0*(double)GHOST + 1.0 + 3.0) * (double)n * (double)N_ITER) / (tf-t0) / 1e9;
            printf("%lf Gflop/s during %lf s\n", gflops, tf-t0);
            printf("Graph took %lf s to create\n", tg-t0);

        } // master

    } // parallel

    return 0;
}
