# include <xkomp/omp.h>
# include <xkomp/xkomp.h>

extern "C"
void
GOMP_taskwait(void)
{
    xkomp_t * xkomp = xkomp_get();
    xkomp->runtime.task_wait();
}

/* See https://github.com/gcc-mirror/gcc/blob/master/libgomp/task.c#L474-L496 */
extern "C"
void
GOMP_task(
    void (*fn) (void *),
    void * data,
    void (*cpyfn) (void *, void *),
    long arg_size,
    long arg_align,
    bool if_clause,
    unsigned flags,
    void ** depend,
    int priority_arg,
    void * detach
) {
    LOGGER_FATAL("TODO");
}

/**
 * GOMP-versioned entry point for omp_target_memcpy_async.
 * The depobj_list uses GOMP's depend representation.
 */
extern "C" int
__gomp_omp_target_memcpy_async(
    void *dst,
    const void *src,
    size_t length,
    size_t dst_offset,
    size_t src_offset,
    int dst_device_num,
    int src_device_num,
    int depobj_count,
    omp_depend_t *depobj_list
) {
    /* TODO: translate GOMP depend objects to xkomp format,
     * then call xkomp_target_memcpy_async() */
    LOGGER_NOT_IMPLEMENTED();
    return -1;
}
__asm__(".symver __gomp_omp_target_memcpy_async, omp_target_memcpy_async@OMP_5.1.1");
