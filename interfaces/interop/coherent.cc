////////////////////////////////////////////////////////////////////
// future work, add a 'coherent' clause so 'depend', 'map' and    //
// 'omp_target_memcpy' behaves 'as if there was an access clause' //
////////////////////////////////////////////////////////////////////

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
