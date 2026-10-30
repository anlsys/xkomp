# include <omp.h>
# include <xkomp/xkomp.h>
# include <assert.h>

////////////////////
// MEMORY MAPPING //
////////////////////

// Extensions

OMP_API(void *, target_map_find_put,
    const int device_id,
    const void * host_ptr,
    const size_t size
) {
    LOGGER_FATAL("TODO");
    return NULL;
}

OMP_API(int, target_map_remove,
   const int device_id,
   const void * host_ptr
) {
    LOGGER_FATAL("TODO");
    return 0;
}

// Standard API

OMP_API(void *, get_mapped_ptr,
    const void * ptr,
    int device_num
) {
    LOGGER_FATAL("TODO");
    return NULL;
}

OMP_API(int, target_is_present,
    const void * ptr,
    int device_num
) {
    LOGGER_FATAL("TODO");
    return 0;
}

OMP_API(int, target_associate_ptr,
    const void * host_ptr,
    const void * device_ptr,
    size_t size,
    size_t device_offset,
    int device_num
) {
    LOGGER_FATAL("TODO");
    return 0;
}

OMP_API(int, target_disassociate_ptr,
    const void * ptr,
    int device_num
) {
    LOGGER_FATAL("TODO");
    return 0;
}

OMP_API(void *, target_alloc,
    size_t size,
    int device_num
) {
    xkomp_t * xkomp = xkomp_get();
    const device_unique_id_t device_unique_id = omp_device_id_to_xkomp(device_num);
    area_chunk_t * chunk = xkomp->runtime.memory_device_allocate(device_unique_id, size);
    return (void *) chunk->ptr;
}

OMP_API(void, target_free,
    void * device_ptr,
    int device_num
) {
    LOGGER_WARN("TODO: xkomp_target_free not implemented, is a noop right now");
}

OMP_API(int, target_memcpy,
    void * dst,
    const void * src,
    size_t length,
    size_t dst_offset,
    size_t src_offset,
    int dst_device_num,
    int src_device_num
) {
    xkomp_t * xkomp = xkomp_get();

    const size_t size = length;
    const device_unique_id_t dst_device_unique_id = omp_device_id_to_xkomp(dst_device_num);
    const device_unique_id_t src_device_unique_id = omp_device_id_to_xkomp(src_device_num);
    const uintptr_t          dst_device_addr      = ((uintptr_t) dst) + dst_offset;
    const uintptr_t          src_device_addr      = ((uintptr_t) src) + src_offset;
    xkomp->runtime.memory_copy(size, dst_device_unique_id, dst_device_addr, src_device_unique_id, src_device_addr);
    return 0;
}

/////////////////////////////
// TARGET MEMORY TRANSFERS //
/////////////////////////////

OMP_API(int, target_memcpy_async,
    void * dst,
    const void * src,
    size_t length,
    size_t dst_offset,
    size_t src_offset,
    int dst_device_num,
    int src_device_num,
    int depobj_count,
    omp_depend_t * depobj_list
) {
    LOGGER_FATAL("Not implemented");
    return -1;
}

