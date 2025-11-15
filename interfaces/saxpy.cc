////////////////// STANDARD OMP ///////////////////////
float λ, * A, * B, * C;

# pragma omp target nowait device(0)            \
   teams distribute parallel for                \
   firstprivate(λ)                              \
   depend(in: A, B) depend(out: C)              \
   map(to: A[0:n], B[0:n]) map(from: C[0:n])
   for (int i = 0; i < n; ++i)
       C[i] = λ * A[i] + B[i];

# pragma omp task depend(in: C)
    func(C, n);

# pragma omp taskwait

/////////////// WITH AN ACCESS CLAUSE //////////////////

float λ, * A, * B, * C;

# pragma omp target nowait device(0)    \
   teams distribute parallel for        \
   firstprivate(λ)                      \
   access(read:  A[0:n], B[0:n])        \
   access(write: C[0:n])
   for (int i = 0; i < n; ++i)
       C[i] = λ * A[i] + B[i];

# pragma omp target update nowait access(release: A[0:n], B[0:n]) device(0)

# pragma omp task access(read: C[0:n])
    func(C, n);

# pragma omp target update nowait access(release: C[0:n]) device(0)

# pragma omp taskwait
