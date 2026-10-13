////////////////////////////////////////////////////////////////////
// future work, add a 'coherent' clause so 'depend', 'map' and    //
// 'omp_target_memcpy' behaves 'as if there was an access clause' //
////////////////////////////////////////////////////////////////////


// 0

x = 0;

# pragma omp target nowait device(0) access(write: x)
    x = 42;

# pragma omp target nowait device(1) access(read: x)
    puts(x);

# pragma omp taskwait

puts(x)        // will print '0', unless we specify that 'taskwait' must write-back all device memory to the host ? Do we want to enforce 'write-back' at the end of some scope ?



// 1

# pragma omp task depend(out: x[0]) coherent
    {}

# pragma omp task access(read: segment(x, 0, 8))
    {}



// 2

# pragma omp target nowait update from(x[0:n]) device(0) coherent

# pragma omp task access(read: x[0:n])
    {}



// 3

// where 'src' is a host pointer
//   and 'dst' a device pointer
omp_target_coherent_memcpy(src, dst, size, src_offset, dst_offset, src_dev, dst_dev);

# pragma omp target nowait access(read: segment(src, 0, size)) device(dst_dev)
    {}
