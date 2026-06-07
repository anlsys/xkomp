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

extern "C"
void *
xkomp_access_pointer(int idx)
{
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    task_t * task = thread->current_task;
    assert(task);
    assert(task->flags & TASK_FLAG_ACCESSES);

    access_t * accesses = TASK_ACCESSES(task);
    assert(accesses);

    access_t * access = accesses + idx;
    assert(access->type == ACCESS_TYPE_SEGMENT);

    return (void *) access->device_view.addr;
}

/////////////////////////
// STANDARD OPENMP API //
/////////////////////////

extern "C"
int
xkomp_get_thread_num(void)
{
    thread_t * tls = thread_t::get_tls();
    assert(tls);

    return tls->tid;
}
EXPORT_OMP_ABI(get_thread_num);

extern "C"
int
xkomp_get_num_threads(void)
{
    thread_t * tls = thread_t::get_tls();
    assert(tls);

    return tls->team->priv.nthreads;
}
EXPORT_OMP_ABI(get_num_threads);

extern "C"
int
xkomp_get_max_threads(void)
{
    int nthreads = xkomp->env.OMP_NUM_THREADS;
    if (nthreads == 0)
    {
        hwloc_topology_t topology;

        hwloc_topology_init(&topology);
        hwloc_topology_load(topology);
        nthreads = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE);
        hwloc_topology_destroy(topology);
    }
    return MIN(nthreads, xkomp->env.OMP_THREAD_LIMIT);
}
EXPORT_OMP_ABI(get_max_threads);

extern "C"
double
xkomp_get_wtime(void)
{
    return get_nanotime() / 1.0e9;
}
EXPORT_OMP_ABI(get_wtime);

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
