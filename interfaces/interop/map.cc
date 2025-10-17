//////////////////////////////////
// Do we want to synchronize ?  //
// Do we want data movement ?   //
//////////////////////////////////

// 1

# pragma omp target nowait update from(x[0:n]) device(0)

# pragma omp task access(read: x[0:n])
    {}

