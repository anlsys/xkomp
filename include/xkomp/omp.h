#ifndef __OMP_H__
# define __OMP_H__

extern "C"
{
    // threading
    int omp_get_thread_num(void);
    int omp_get_num_threads(void);

    // devices
    void omp_set_default_device(int device);
    int  omp_get_default_device(void);
    int  omp_get_num_devices(void);
    int  omp_get_device_num(void);
    int  omp_get_initial_device(void);
    int  omp_is_initial_device(void);
};

 #endif /* __OMP_H__ */
