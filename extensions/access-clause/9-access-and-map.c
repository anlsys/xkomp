int x[4];
int y[4];

// NON-CONFORMING (access and map clauses present on the same base pointer)
# pragma omp target loop nowait access(write: x[0:4]) map(to: x[0:4])
for (int i = 0 ; i < 4 ; ++i)
    x[i] = i;

// CONFORMING, PRESERVING EXISTING SEMANTICS.
# pragma omp target loop nowait access(write: x[0:4]) map(to: y[0:4])
for (int i = 0 ; i < 4 ; ++i)
    x[i] = y[i];
