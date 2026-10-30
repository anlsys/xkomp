# include <omp.h>
# include <xkomp/xkomp.h>
# include <assert.h>

/* In OpenMP, non-host devices are indexed from 0 to n-1 where 'n' is the value
 * returned by 'omp_get_num_devices' */

OMP_API(void, set_default_device,
    int device
) {
    LOGGER_NOT_IMPLEMENTED();
}

OMP_API(int, get_default_device)
{
    return 0;
}

OMP_API(int, get_num_devices)
{
    xkomp_t * xkomp = xkomp_get();
    return xkomp->runtime.get_ndevices() - 1;
}

OMP_API(int, get_device_num)
{
    thread_t * thread = thread_t::get_tls();
    return (int) (thread->device_unique_id == XKRT_HOST_DEVICE_UNIQUE_ID ? xkomp_get_initial_device() : thread->device_unique_id - 1);
}

OMP_API(int, get_initial_device)
{
    return xkomp_get_num_devices();
}

OMP_API(int, is_initial_device)
{
    return xkomp_get_device_num() == xkomp_get_initial_device();
}

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
