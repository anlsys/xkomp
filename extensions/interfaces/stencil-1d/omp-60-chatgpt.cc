/*
 * 1D heat equation (explicit Euler) using an OpenMP target stencil.
 * Targets all available OpenMP devices by splitting the 1D domain into device chunks.
 *
 * Compile:
 *   Use an OpenMP-capable compiler with target offload support.
 *   Example (GCC with offload support / vendor toolchain may vary):
 *     gcc -fopenmp -O3 -march=native -o heat1d_openmp_target heat1d_openmp_target.c
 *
 * Run:
 *   ./heat1d_openmp_target [N] [timesteps] [alpha] [dx]
 *   Example:
 *   ./heat1d_openmp_target 100000 1000 0.01 0.001
 *
 * Notes:
 * - The program queries omp_get_num_devices() and splits the domain accordingly.
 * - It uses Dirichlet zero boundary conditions at both ends.
 * - Mapping uses array sections (u[offset:len]) for minimal data transfer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

/* Utility: clamp */
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

int main(int argc, char **argv) {
    /* Problem parameters (can be overridden from command line) */
    int N = 100000;          /* number of grid points */
    int steps = 1000;        /* number of time steps */
    double alpha = 0.01;     /* thermal diffusivity */
    double dx = 1.0 / (N-1); /* spatial step */
    double dt = 0.25 * dx * dx / alpha; /* stability: choose dt <= dx^2/(2*alpha) -> we pick a safe fraction */

    if (argc >= 2) N = atoi(argv[1]);
    if (argc >= 3) steps = atoi(argv[2]);
    if (argc >= 4) alpha = atof(argv[3]);
    if (argc >= 5) dx = atof(argv[4]);

    /* recompute dt after potential parameter changes */
    dt = 0.2 * dx * dx / alpha; /* conservative CFL factor */

    printf("1D heat equation: N=%d, steps=%d, alpha=%g, dx=%g, dt=%g\n", N, steps, alpha, dx, dt);

    int num_devices = omp_get_num_devices();
    if (num_devices <= 0) {
        printf("No OpenMP devices detected (omp_get_num_devices() == %d). Exiting.\n", num_devices);
        return 1;
    }
    printf("Detected %d OpenMP device(s). Partitioning domain across devices.\n", num_devices);

    /* Allocate arrays on host */
    double *u = (double*)malloc(sizeof(double) * N);
    double *u_new = (double*)malloc(sizeof(double) * N);
    if (!u || !u_new) {
        fprintf(stderr, "Allocation failed\n");
        return 1;
    }

    /* Initialize: example initial condition - Gaussian centered or hot spot */
    double center = 0.5;
    for (int i = 0; i < N; ++i) {
        double x = (double)i * dx;
        /* Gaussian centered at center with width 0.05 */
        u[i] = exp(-200.0 * (x - center) * (x - center));
    }

    /* Pre-calc coefficient */
    double coeff = alpha * dt / (dx * dx);

    /* Determine partition sizes (contiguous) */
    int base_chunk = N / num_devices;
    int remainder = N % num_devices;

    /* device chunk ranges: start index and length for each device */
    int *dev_start = (int*)malloc(sizeof(int) * num_devices);
    int *dev_len   = (int*)malloc(sizeof(int) * num_devices);

    int idx = 0;
    for (int d = 0; d < num_devices; ++d) {
        int len = base_chunk + (d < remainder ? 1 : 0);
        dev_start[d] = idx;
        dev_len[d] = len;
        idx += len;
    }

    /* Temporary host buffer for printing progress (optional) */
    printf("Device partitions (start:length):\n");
    for (int d = 0; d < num_devices; ++d) {
        printf("  dev %d: %d : %d\n", d, dev_start[d], dev_len[d]);
    }

    /* Time-stepping loop */
    for (int t = 0; t < steps; ++t) {

        /* Offload each chunk to its device. We use an OpenMP parallel loop over devices
           so offloads to different devices can proceed concurrently (if supported). */
        #pragma omp parallel for schedule(static)
        for (int d = 0; d < num_devices; ++d) {
            int start = dev_start[d];
            int len = dev_len[d];
            int end = start + len - 1;

            /* Determine halo (ghost) indices we need to map:
               - if not global left boundary, need left neighbor (index start-1)
               - if not global right boundary, need right neighbor (index end+1)
               We'll map the host array section starting at halo_left to halo_len elements.
            */
            int halo_left = (start > 0) ? (start - 1) : start;
            int halo_len = len + ((start > 0) ? 1 : 0) + ((end < N-1) ? 1 : 0);
            int left_offset = start - halo_left; /* 0 if start==halo_left else 1 */

            /* Map array section u[halo_left : halo_len] to device, map from u_new[start : len] back */
            #pragma omp target teams distribute parallel for                                \
                map(to: u[halo_left:halo_len])                                              \
                map(from: u_new[start:len]) device(d)
            for (int local = 0; local < len; ++local) {
                /* local indexes into the mapped u section:
                   global index = start + local
                   u_dev array layout: u_dev[0] corresponds to u[halo_left]
                   left neighbor in u_dev is at index (left_offset + local - 1)
                */
                int global_i = start + local;

                /* left neighbor */
                double ul;
                if (global_i == 0) {
                    /* left global boundary (Dirichlet zero) */
                    ul = 0.0;
                } else {
                    /* available in mapped halo or interior */
                    ul = u[halo_left + left_offset + local - 1];
                }

                /* center */
                double uc = u[halo_left + left_offset + local];

                /* right neighbor */
                double ur;
                if (global_i == N-1) {
                    /* right global boundary (Dirichlet zero) */
                    ur = 0.0;
                } else {
                    /* right neighbor is at offset +1 */
                    ur = u[halo_left + left_offset + local + 1];
                }

                /* stencil update (explicit Euler) */
                u_new[global_i] = uc + coeff * (ul - 2.0 * uc + ur);
            }
        } /* end parallel for over devices */

        /* swap arrays */
        double *tmp = u;
        u = u_new;
        u_new = tmp;

        /* optional: print progress or probes every so often */
        if ((t % (steps / 10 + 1)) == 0) {
            /* print center value */
            int mid = N / 2;
            double maxv = 0.0;
            double max_loc = 0.0;
            for (int i = 0; i < N; ++i) {
                double v = fabs(u[i]);
                if (v > maxv) { maxv = v; max_loc = u[i]; }
            }
            printf("t=%5d / %d  center u[%d]=%g  max_abs=%g\n", t, steps, mid, u[mid], maxv);
        }
    } /* end time loop */

    /* Write final result to file (so user can inspect or plot) */
    FILE *f = fopen("final_u.dat", "w");
    if (f) {
        for (int i = 0; i < N; ++i) {
            double x = (double)i * dx;
            fprintf(f, "%g %g\n", x, u[i]);
        }
        fclose(f);
        printf("Final profile written to final_u.dat\n");
    } else {
        fprintf(stderr, "Couldn't open final_u.dat for writing\n");
    }

    /* Print a small sample to stdout */
    printf("Sample of final values (first 10 and last 10):\n");
    for (int i = 0; i < 10 && i < N; ++i) printf("u[%d]=%g ", i, u[i]);
    printf("\n");
    for (int i = N-10; i < N; ++i) if (i>=0) printf("u[%d]=%g ", i, u[i]);
    printf("\n");

    /* free memory */
    free(u);
    free(u_new);
    free(dev_start);
    free(dev_len);

    return 0;
}

