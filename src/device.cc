# include <xkomp/xkomp.h>
# include <assert.h>

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
    LOGGER_NOT_IMPLEMENTED();
    return -1;
}

extern "C"
int
omp_get_num_devices(void)
{
    xkomp_t * xkomp = xkomp_get();
    return xkomp->runtime.get_ndevices() - 1;
}

extern "C"
int
omp_get_device_num(void)
{
    LOGGER_NOT_IMPLEMENTED();
    return -1;
}

extern "C"
int
omp_get_initial_device(void)
{
    return HOST_DEVICE_GLOBAL_ID - 1;
}

extern "C"
int
omp_is_initial_device(void)
{
    return omp_get_num_devices() == omp_get_initial_device();
}
