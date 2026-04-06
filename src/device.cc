# include <xkomp/xkomp.h>
# include <assert.h>

/* In OpenMP, non-host devices are indexed from 0 to n-1 where 'n' is the value
 * returned by 'omp_get_num_devices' */

extern "C"
void
omp_set_default_device(int device)
{
    LOGGER_NOT_IMPLEMENTED();
}

extern "C"
int
omp_get_default_device(void)
{
    return 0;
}

extern "C"
int
omp_get_num_devices(void)
{
    xkomp_t * xkomp = xkomp_get();
    return xkomp->runtime.get_ndevices();
}

extern "C"
int omp_get_initial_device(void);

extern "C"
int
omp_get_device_num(void)
{
    thread_t * thread = thread_t::get_tls();
    return (int) (thread->device_unique_id == XKRT_HOST_DEVICE_UNIQUE_ID ? omp_get_initial_device() : thread->device_unique_id - 1);
}

extern "C"
int
omp_get_initial_device(void)
{
    return omp_get_num_devices();
}

extern "C"
int
omp_is_initial_device(void)
{
    return omp_get_device_num() == omp_get_initial_device();
}

extern "C"
xkrt_device_unique_id_t
omp_device_id_to_xkomp(int device_id)
{
    return (device_id + 1) % omp_get_num_devices();
}

extern "C"
int
xkomp_device_unique_id_to_omp(xkrt_device_unique_id_t device_unique_id)
{
    return (device_unique_id == XKRT_HOST_DEVICE_UNIQUE_ID) ? omp_get_num_devices() : (int) (device_unique_id - 1);
}
