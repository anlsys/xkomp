//////////////////////////////////
// Do we want to synchronize ?  //
// Do we want data movement ?   //
//////////////////////////////////

// 1    - ref count issues

# pragma omp target enter data map(to: x[0:n]) device(0)

# pragma omp target nowait access(read: x[0:n]) device(0)
    {}

# pragma omp target exit data map(storage: x[0:n]) device(0)

# pragma omp ____ x[0:n]    





# pragma omp target map(to: x[0:n]) device(0)
    {}
