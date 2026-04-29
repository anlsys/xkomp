# include <xkomp/xkomp.h>
# include <xkomp/support.h>

# include <xkrt/logger/logger.h>
# include <stdint.h>

static xkomp_t * xkomp = NULL;

extern "C"
xkomp_t *
xkomp_get(void)
{
    if (xkomp == NULL)
    {
        xkomp = (xkomp_t *) malloc(sizeof(xkomp_t));
        assert(xkomp);
        xkomp->runtime.init();
        xkomp_env_init(&xkomp->env);
        xkomp_task_register_format(xkomp);
        new (&xkomp->taskgraphs) std::map<xkomp_taskgraph_id_t, xkomp_taskgraph_t>();
    }

    return xkomp;
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
    return MIN(xkomp->env.OMP_NUM_THREADS, xkomp->env.OMP_THREAD_LIMIT);
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
    assert(xkomp);
    xkomp->runtime.deinit();
    xkomp->taskgraphs.~map();
    free(xkomp);
    xkomp = NULL;
}


