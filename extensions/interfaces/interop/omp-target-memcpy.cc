//////////////////////////////////////
// Do we want data movement twice ? //
//////////////////////////////////////

// 1

// where 'src' is a host pointer
//   and 'dst' a device pointer
omp_target_memcpy(src, dst, size, src_offset, dst_offset, src_dev, dst_dev);

# pragma omp target nowait access(read: segment(src, 0, size)) device(dst_dev)
    {}

