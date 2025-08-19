# include <xkomp/xkomp.h>
# include <xkomp/support.h>
# include <xkomp/kmp.h>

# include <xkrt/logger/logger.h>
# include <stdint.h>


xkomp_t  _xkomp;
xkomp_t * xkomp;

extern "C"
xkomp_t *
xkomp_get(void)
{
    if (xkomp == NULL)
    {
        xkomp = &_xkomp;
        xkrt_init(&xkomp->runtime);
        xkomp_env_init(&xkomp->env);
        xkomp_task_register_format(xkomp);
    }

    return xkomp;
}

extern "C"
kmp_int32
__kmpc_global_thread_num(ident_t * loc)
{
    (void) loc;

    // ensure runtime is initialized
    xkomp_get();

    xkrt_thread_t * tls = xkrt_thread_t::get_tls();
    assert(tls);

    return tls->gtid;
}

/////////////////////////
// STANDARD OPENMP API //
/////////////////////////

extern "C"
int
omp_get_thread_num(void)
{
    xkrt_thread_t * tls = xkrt_thread_t::get_tls();
    assert(tls);

    return tls->tid;
}

extern "C"
int
omp_get_num_threads(void)
{
    xkrt_thread_t * tls = xkrt_thread_t::get_tls();
    assert(tls);

    return tls->team->priv.nthreads;
}

extern "C"
int
omp_get_max_threads(void)
{
    xkomp_t * omp = xkomp_get();
    return MIN(omp->env.OMP_NUM_THREADS, omp->env.OMP_THREAD_LIMIT);
}

extern "C"
double
omp_get_wtime(void)
{
    return xkrt_get_nanotime() / 1.0e9;
}
