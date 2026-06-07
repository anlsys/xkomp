# include <xkomp/kmp.h>
# include <xkomp/omp.h>
# include <xkomp/xkomp.h>
# include <xkomp/support.h>

# include <xkrt/thread/thread.h>

extern "C"
int
__kmpc_get_target_offload(void)
{
    # if XKOMP_SUPPORT_TARGET
    return 1;
    # else
    return 0;
    # endif
}

/* check if the current thread has a team */
extern "C"
bool
__kmpc_omp_has_task_team(int gtid)
{
    thread_t * tls = thread_t::get_tls();
    assert(tls);
    assert(gtid == tls->gtid);
    return tls->team != NULL;
}

extern "C"
void **
__kmpc_omp_get_target_async_handle_ptr(kmp_int32 gtid)
{
    return NULL;
}

/**
 * KMP-versioned entry point for omp_target_memcpy_async.
 * The depobj_list uses KMP's depend representation (kmp_depend_info_t).
 */
extern "C" int
__kmp_omp_target_memcpy_async(
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
    /* TODO: translate KMP depend objects to xkomp format,
     * then call xkomp_target_memcpy_async() */
    LOGGER_NOT_IMPLEMENTED();
    return -1;
}
__asm__(".symver __kmp_omp_target_memcpy_async, omp_target_memcpy_async@VERSION");
