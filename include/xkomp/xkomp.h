#ifndef __XKOMP_H__
# define __XKOMP_H__

# include <xkrt/runtime.h>
# include <xkomp/support.h>
# include <xkomp/taskgraph.h>

XKRT_NAMESPACE_USE;

/* environment variables parsed at program starts */
typedef struct  xkomp_env_t
{
    char OMP_DISPLAY_ENV;
    int OMP_NUM_THREADS;
    int OMP_THREAD_LIMIT;
    const char * OMP_PLACES;
    const char * OMP_PROC_BIND;
}               xkomp_env_t;

/** global variable that holds the entire openmp context */
typedef struct  xkomp_t
{
    /* underlaying XKRT runtime */
    runtime_t runtime;

    /* omp task format */
    task_format_id_t task_format;

    /* environment variables */
    xkomp_env_t env;

    /**
     *  Taskgraphs
     *  Parallel init/replay of the same taskgraph is not supported
     */
    std::map<xkomp_taskgraph_id_t, xkomp_taskgraph_t> taskgraphs;

}               xkomp_t;

extern "C"
xkomp_t * xkomp_get(void);

extern "C"
xkrt_device_unique_id_t omp_device_id_to_xkomp(int device_id);

extern "C"
int xkomp_device_unique_id_to_omp(xkrt_device_unique_id_t device_unique_id);

/** load env variables */
void xkomp_env_init(xkomp_env_t * env);

/* save task format */
void xkomp_task_register_format(xkomp_t * xkomp);

/* microtask max arguments */
# define XKOMP_MICROTASK_MAX_ARGS 1024

/* # pragma omp parallel */
extern "C"
void xkomp_parallel(unsigned int nthreads, team_routine_t routine, void * args);

# endif /* __XKOMP_H__ */
