#ifndef __OMP_H__
# define __OMP_H__

/**
 * For each omp_<suffix> function, declare both the omp_<suffix> and
 * xkomp_<suffix> prototypes.  The actual symbol versioning is handled by
 * the EXPORT_OMP_ABI macro in the .cc files; the declarations here let
 * callers use either name.
 */
#define DECLARE_OMP_ABI(ret, suffix, ...) \
    ret omp_##suffix(__VA_ARGS__);        \
    ret xkomp_##suffix(__VA_ARGS__)

extern "C"
{
    // threading
    DECLARE_OMP_ABI(int, get_thread_num, void);
    DECLARE_OMP_ABI(int, get_num_threads, void);
    DECLARE_OMP_ABI(int, get_max_threads, void);

    // timing
    DECLARE_OMP_ABI(double, get_wtime, void);

    // devices
    DECLARE_OMP_ABI(void, set_default_device, int device);
    DECLARE_OMP_ABI(int,  get_default_device, void);
    DECLARE_OMP_ABI(int,  get_num_devices, void);
    DECLARE_OMP_ABI(int,  get_device_num, void);
    DECLARE_OMP_ABI(int,  get_initial_device, void);
    DECLARE_OMP_ABI(int,  is_initial_device, void);
};

#undef DECLARE_OMP_ABI

#endif /* __OMP_H__ */
