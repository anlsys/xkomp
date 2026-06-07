# include <xkomp/omp.h>
# include <xkomp/xkomp.h>
# include <assert.h>

/* In OpenMP, non-host devices are indexed from 0 to n-1 where 'n' is the value
 * returned by 'omp_get_num_devices' */

extern "C"
void
xkomp_set_default_device(int device)
{
    LOGGER_NOT_IMPLEMENTED();
}
EXPORT_OMP_ABI(set_default_device);

extern "C"
int
xkomp_get_default_device(void)
{
    return 0;
}
EXPORT_OMP_ABI(get_default_device);

extern "C"
int
xkomp_get_num_devices(void)
{
    xkomp_t * xkomp = xkomp_get();
    return xkomp->runtime.get_ndevices() - 1;
}
EXPORT_OMP_ABI(get_num_devices);

extern "C"
int xkomp_get_initial_device(void);

extern "C"
int
xkomp_get_device_num(void)
{
    thread_t * thread = thread_t::get_tls();
    return (int) (thread->device_unique_id == XKRT_HOST_DEVICE_UNIQUE_ID ? xkomp_get_initial_device() : thread->device_unique_id - 1);
}
EXPORT_OMP_ABI(get_device_num);

extern "C"
int
xkomp_get_initial_device(void)
{
    return xkomp_get_num_devices();
}
EXPORT_OMP_ABI(get_initial_device);

extern "C"
int
xkomp_is_initial_device(void)
{
    return xkomp_get_device_num() == xkomp_get_initial_device();
}
EXPORT_OMP_ABI(is_initial_device);

/////////////////////////////
// TARGET MEMORY TRANSFERS //
/////////////////////////////

extern "C"
int
xkomp_target_memcpy_async(
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
    /* TODO: implement */
    LOGGER_NOT_IMPLEMENTED();
    return -1;
}

/**
 * OMP_API_VERSION entry point for omp_target_memcpy_async.
 * This is the default-versioned symbol for direct user calls
 * against xkomp's omp.h / omp_depend_t.
 *
 * extern "C" prevents C++ name mangling so .symver can reference
 * the symbol by its plain name.  The version script's local: *
 * hides the unversioned __xkomp_omp_target_memcpy_async symbol.
 */
extern "C" int
__xkomp_omp_target_memcpy_async(
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
    /* TODO: translate omp_depend_t to xkomp format */
    return xkomp_target_memcpy_async(dst, src, length, dst_offset, src_offset,
                                     dst_device_num, src_device_num,
                                     depobj_count, depobj_list);
}
__asm__(".symver __xkomp_omp_target_memcpy_async, omp_target_memcpy_async@@OMP_API_VERSION");

/////////////////////
// XKOMP EXTENSION //
/////////////////////

extern "C"
xkrt_device_unique_id_t
omp_device_id_to_xkomp(int device_id)
{
    static_assert(XKRT_HOST_DEVICE_UNIQUE_ID == 0);
    if (device_id == xkomp_get_initial_device())
        return XKRT_HOST_DEVICE_UNIQUE_ID;
    return 1 + (device_id % xkomp_get_num_devices());
}

extern "C"
int
xkomp_device_unique_id_to_omp(xkrt_device_unique_id_t device_unique_id)
{
    return (device_unique_id == XKRT_HOST_DEVICE_UNIQUE_ID) ? xkomp_get_num_devices() : (int) (device_unique_id - 1);
}
