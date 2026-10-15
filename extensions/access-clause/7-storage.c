int x[4];

// May preallocate a non-coherent replica
# pragma omp target_enter_data nowait device(0) access(storage: x[0:4])

// May allocate, may trigger H2D
# pragma omp target loop nowait device(0) access(read: x[0:4])
for (int i = 0 ; i < 4 ; ++i)
    x[i] = i;

// Must deallocate all replicas
# pragma omp target_exit_data nowait device(0) access(storage: x[0:4])
