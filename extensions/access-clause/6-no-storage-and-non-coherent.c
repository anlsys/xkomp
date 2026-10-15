int x[4];

// Synchronizes, no allocation, no coherence, must not be referenced
# pragma omp target loop nowait access(no_storage, read: x[0:4])
for (int i = 0 ; i < 4 ; ++i)
    x[i] = i;   // SEGV

// Synchronizes, allocates, no coherence, can be referenced
# pragma omp target loop nowait access(non_coherent, read: x[0:4])
for (int i = 0 ; i < 4 ; ++i)
    x[i] = i;   // CORRECT
