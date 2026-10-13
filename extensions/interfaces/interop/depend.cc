/////////////////////////////////
// Do we want to synchronize ? //
/////////////////////////////////


// 1

# pragma omp task depend(out: x[0])
    {}

# pragma omp task access(read: segment(x, 0, 8))
    {}




// 2

# pragma omp task depend(out: x[0])
    {}

# pragma omp task access(read: segment(x, 2, 8))
    {}




// 3

# pragma omp task depend(out: x[4])
    {}

# pragma omp task access(read: segment(x, 2, 8))
    {}







