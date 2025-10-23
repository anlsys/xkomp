/////////////////////////////////
// Do we want to synchronize ? //
/////////////////////////////////

// 0

x = 0;

# pragma omp target nowait device(0) access(write: x)
    x = 42;

# pragma omp target nowait device(1) access(read: x)
    puts(x);

# pragma omp taskwait

puts(x)



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







