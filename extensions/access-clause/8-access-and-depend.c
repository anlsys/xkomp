int x[4];

////////////////
//  EXAMPLE 1 //
////////////////

# pragma omp task depend(out: x[0:4])   // T1
    {}

# pragma omp task depend(in: x[2:2])    // T2 is non conforming
    {}

////////////////
//  EXAMPLE 2 //
////////////////

# pragma omp task depend(out: x[0:4])                   // T1
    {}

# pragma omp task depend(in: x[0:4])                    // T2 depends on T1
    {}

# pragma omp target loop nowait access(read: x[0:4])    // T3 is independent
for (int i = 0 ; i < 4 ; ++i);

////////////////
//  EXAMPLE 3 //
////////////////

# pragma omp task depend(out: x[0:4])                                   // T1
    {}

# pragma omp task depend(in: x[0:4])                                    // T2 depends on T1
    {}

# pragma omp target loop nowait access(read: x[0:4]) depend(in: x[0:4]) // T3 depends on T2
for (int i = 0 ; i < 4 ; ++i);
