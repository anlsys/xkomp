/////////////////////////////////
// Do we want to synchronize ? //
/////////////////////////////////

// 0'

int x[1];
x[0] = 0;

# pragma omp enter data map(storage: x[0:1]) device(0)
# pragma omp enter data map(storage: x[0:1]) device(1)

# pragma omp target nowait update to(x[0:1]) depend(out: x)

# pragma omp target nowait device(0) depend(out: x)
    x = 42;

omp_depend_t deps[] = {
    ...
};

void * src = omp_get_mapped_ptr(x, 0);
void * dst = omp_get_mapped_ptr(x, 1);
omp_target_memcpy_async(dst, src, ..., deps, sizeof(deps)/sizeof(omp_depend_t));

# pragma omp target nowait device(1) depend(in: x)
    puts(x);

# pragma omp taskwait

puts(x)


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







