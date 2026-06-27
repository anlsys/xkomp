#ifndef __XKOMP_H__
# define __XKOMP_H__

# include <xkrt/runtime.h>
# include <xkrt/data-structures/small-vector.h>
# include <xkomp/support.h>
# include <xkomp/taskgraph.h>

/**
 * EXPORT_OMP_ABI(suffix)
 *
 * Place this macro after a function defined as `xkomp_<suffix>(...)`.
 * It automatically exports three symbols from a single implementation:
 *
 *   1. xkomp_<suffix>          — native XKOMP symbol
 *   2. omp_<suffix>@VERSION    — KMP-compatible versioned symbol
 *   3. omp_<suffix>@@OMP_API_VERSION — default OMP API versioned symbol
 *
 * Example:
 *   extern "C" int xkomp_get_num_threads(void) { ... }
 *   EXPORT_OMP_ABI(get_num_threads);
 */
#define EXPORT_OMP_ABI(suffix) \
    extern "C" __typeof__(xkomp_##suffix) __omp_api_##suffix \
        __attribute__((alias("xkomp_" #suffix))); \
    __asm__(".symver __omp_api_" #suffix ", omp_" #suffix "@@OMP_API_VERSION"); \
    extern "C" __typeof__(xkomp_##suffix) __omp_kmp_##suffix \
        __attribute__((alias("xkomp_" #suffix))); \
    __asm__(".symver __omp_kmp_" #suffix ", omp_" #suffix "@VERSION")

XKRT_NAMESPACE_USE;

/* environment variables parsed at program starts */
typedef struct  xkomp_env_t
{
    char OMP_DISPLAY_ENV;
    int OMP_NUM_THREADS;
    int OMP_THREAD_LIMIT;
    const char * OMP_PLACES;
    const char * OMP_PROC_BIND;

    /* set of cgir command-graph optimization passes applied to recorded
     * taskgraphs (see OMP_TASKGRAPH_OPT in env.cc) */
    cgir::command_graph_pass_set_t OMP_TASKGRAPH_OPT;
}               xkomp_env_t;

/* a persistent team cached for `# pragma omp parallel`, reused across regions
 * that request the same number of threads */
typedef struct  xkomp_team_entry_t
{
    int nthreads;
    team_t team;
}               xkomp_team_entry_t;

/* max number of distinct thread counts cached at once. Teams are stored in-place
 * in the small vector, so this is its inline capacity: growing beyond it would
 * relocate the teams and dangle their worker threads. We never expect more than
 * a couple of distinct thread counts. */
# define XKOMP_MAX_CACHED_TEAMS 8

/** global variable that holds the entire openmp context */
typedef struct  xkomp_t
{
    /* underlaying XKRT runtime */
    runtime_t runtime;

    /* omp task format */
    struct {
        struct {
            task_format_id_t host;
            task_format_id_t target_memcpy_async;
        } kmp;
    } formats;

    /* environment variables */
    xkomp_env_t env;

    /**
     *  Taskgraphs
     *  Parallel init/replay of the same taskgraph is not supported
     */
    std::map<xkomp_taskgraph_id_t, xkomp_taskgraph_t> taskgraphs;

    /**
     *  Persistent teams for `# pragma omp parallel`, cached by number of
     *  threads and reused across regions to avoid respawning pthreads. The
     *  teams live in-place inside the small vector (no heap allocation).
     */
    small_vector_t<xkomp_team_entry_t, XKOMP_MAX_CACHED_TEAMS> teams;

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
void xkomp_task_register_formats(xkomp_t * xkomp);
void xkomp_task_register_formats_kmp(xkomp_t * xkomp);
void xkomp_task_register_formats_kmp_task(xkomp_t * xkomp);
void xkomp_task_register_formats_kmp_target(xkomp_t * xkomp);

/* microtask max arguments */
# define XKOMP_MICROTASK_MAX_ARGS 1024

/* # pragma omp parallel */
extern "C"
void xkomp_parallel(unsigned int nthreads, team_routine_t routine, void * args);

# endif /* __XKOMP_H__ */
