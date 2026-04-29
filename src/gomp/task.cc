# include <omp.h>
# include <xkomp/xkomp.h>

extern "C"
void
GOMP_taskwait(void)
{
    xkomp_t * xkomp = xkomp_get();
    xkomp->runtime.task_wait();
}

/* See https://github.com/gcc-mirror/gcc/blob/master/libgomp/task.c#L474-L496 */
extern "C"
void
GOMP_task(
    void (*fn) (void *),
    void * data,
    void (*cpyfn) (void *, void *),
    long arg_size,
    long arg_align,
    bool if_clause,
    unsigned flags,
    void ** depend,
    int priority_arg,
    void * detach
) {
    LOGGER_FATAL("TODO");
}
