# include <xkrt/logger/logger.h>

# include <assert.h>
# include <kmp.h>

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
    LOGGER_NOT_IMPLEMENTED();
    return -1;
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
    LOGGER_NOT_IMPLEMENTED();
    return -1;
}

extern "C"
int
omp_is_initial_device(void)
{
    LOGGER_NOT_IMPLEMENTED();
    return 0;
}
