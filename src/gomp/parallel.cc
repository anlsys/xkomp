# include <omp.h>
# include <xkomp/xkomp.h>

typedef struct  wargs_t
{
    void (*fn) (void *);
    void * data;
}               wargs_t;

static void *
fork_call_wrapper(
    runtime_t * runtime,
    team_t * team,
    thread_t * thread
) {
    assert(thread);

    wargs_t * wargs = (wargs_t *) thread->team->desc.args;
    wargs->fn(wargs->data);

    xkomp_t * omp = xkomp_get();
    omp->runtime.team_barrier<true>(thread->team, thread);

    return NULL;
}

extern "C"
void
GOMP_parallel(
    void (*fn) (void *),
    void *data,
    unsigned num_threads,
    unsigned int flags
) {
    (void) flags;
    LOGGER_WARN("flags not supported");

    wargs_t wargs;
    wargs.fn = fn;
    wargs.data = data;
    xkomp_parallel(num_threads, (team_routine_t) fork_call_wrapper, &wargs);
}

extern "C"
bool
GOMP_single_start(void)
{
    thread_t * tls = thread_t::get_tls();
    assert(tls);
    return tls->tid == 0;
}
