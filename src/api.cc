# include <xkomp/xkomp.h>
# include <xkomp/support.h>
# include <xkomp/kmp.h>

# include <xkrt/logger/logger.h>
# include <stdint.h>

extern "C"
xkomp_t *
xkomp_get(void)
{
    static xkomp_t * xkomp = NULL;
    if (xkomp == NULL)
    {
        xkomp = (xkomp_t *) malloc(sizeof(xkomp_t));
        assert(xkomp);
        xkomp->runtime.init();
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

    thread_t * tls = thread_t::get_tls();
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
    thread_t * tls = thread_t::get_tls();
    assert(tls);

    return tls->tid;
}

extern "C"
int
omp_get_num_threads(void)
{
    thread_t * tls = thread_t::get_tls();
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
    return get_nanotime() / 1.0e9;
}

///////////////////////////////////////
// init/deinit of the shared library //
///////////////////////////////////////

void __attribute__((constructor))
__xkomp_init(void)
{
    xkomp_get();
}

void __attribute__((destructor))
__xkomp_teardown(void)
{
    xkomp_t * xkomp = xkomp_get();
    xkomp->runtime.deinit();
}


