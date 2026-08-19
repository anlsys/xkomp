/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Thierry Gautier, thierry.gautier@inrialpes.fr
** Romain PEREIRA, romain.pereira@inria.fr + rpereira@anl.gov
**
** This software is a computer program whose purpose is to execute
** blas subroutines on multi-GPUs system.
**
** This software is governed by the CeCILL-C license under French law and
** abiding by the rules of distribution of free software.  You can  use,
** modify and/ or redistribute the software under the terms of the CeCILL-C
** license as circulated by CEA, CNRS and INRIA at the following URL
** "http://www.cecill.info".

** As a counterpart to the access to the source code and  rights to copy,
** modify and redistribute granted by the license, users are provided only
** with a limited warranty  and the software's author,  the holder of the
** economic rights,  and the successive licensors  have only  limited
** liability.

** In this respect, the user's attention is drawn to the risks associated
** with loading,  using,  modifying and/or developing or reproducing the
** software by the user in light of its specific status of free software,
** that may mean  that it is complicated to manipulate,  and  that  also
** therefore means  that it is reserved for developers  and  experienced
** professionals having in-depth computer knowledge. Users are therefore
** encouraged to load and test the software's suitability as regards their
** requirements in conditions enabling the security of their systems and/or
** data to be ensured and,  more generally, to use and operate it in the
** same conditions as regards security.

** The fact that you are presently reading this means that you have had
** knowledge of the CeCILL-C license and that you accept its terms.
**/

# ifndef _GNU_SOURCE
#  define _GNU_SOURCE   /* sched_getcpu, strdup, strtok_r */
# endif

# include <xkomp/xkomp.h>   /* pulls <xkomp/ompt.h> (helper decls) when OMPT is enabled */
# include <xkomp/support.h>

# if XKRT_SUPPORT_TOOLS && XKOMP_SUPPORT_OMPT

/* Reference the tool's entry point weakly (it may be absent). The pragma must
 * precede the declaration in <omp-tools.h>, so a tool present in the current
 * address space (linked / LD_PRELOAD'd) resolves it; otherwise it stays NULL
 * and we fall back to OMP_TOOL_LIBRARIES. */
# pragma weak ompt_start_tool

# include <omp-tools.h>

# include <xkrt/logger/logger.h>

# include <atomic>
# include <cstdint>
# include <cstdlib>
# include <cstring>

# include <dlfcn.h>
# include <sched.h>
# include <unistd.h>

XKRT_NAMESPACE_USE;

/* type of the entry point a tool exports (not part of the spec header) */
typedef ompt_start_tool_result_t * (*ompt_start_tool_t)(unsigned int, const char *);

/* the OpenMP version advertised to the tool (5.0) */
# define XKOMP_OMPT_VERSION 201811

/* the host device number passed to the tool's initialize */
# define XKOMP_OMPT_INITIAL_DEVICE 0

/* xkrt_tool_data_t and ompt_data_t are both 64-bit unions {uint64_t;void*} */
static_assert(sizeof(ompt_data_t) == sizeof(xkrt_tool_data_t),
              "ompt_data_t and xkrt_tool_data_t must be layout-compatible");

/* ------------------------------------------------------------------------- */
/* OMPT state (one tool per process)                                         */
/* ------------------------------------------------------------------------- */

/* the callback table, indexed by ompt_callbacks_t (1..ompt_callback_error) */
# define XKOMP_OMPT_CB_MAX (ompt_callback_error + 1)
static ompt_callback_t          OMPT_CB[XKOMP_OMPT_CB_MAX] = { NULL };

/* whether an OMPT tool is active */
static bool                     OMPT_ENABLED = false;

/* the tool description + its dlopen handle (if loaded from OMP_TOOL_LIBRARIES) */
static ompt_start_tool_result_t * OMPT_TOOL = NULL;
static void *                     OMPT_TOOL_DLHANDLE = NULL;

/* per (OS) thread OMPT data + bookkeeping */
static thread_local ompt_data_t TLS_THREAD_DATA   = { .value = 0 };
static thread_local bool        TLS_THREAD_BEGUN   = false;
static thread_local bool        TLS_INITIAL_TASK   = false;

/* process-wide unique id counter */
static std::atomic<uint64_t>    OMPT_UID{1};

/* reinterpret an XKRT tool-data slot as an OMPT data slot (layout-identical) */
static inline ompt_data_t *
as_ompt(xkrt_tool_data_t * d)
{
    return reinterpret_cast<ompt_data_t *>(d);
}

/* invoke callback EVENT (of concrete type TYPE) if registered */
# define OMPT_CALL(EVENT, TYPE, ...)                                            \
    do {                                                                        \
        ompt_callback_t __f = OMPT_CB[EVENT];                                   \
        if (__f) ((TYPE) __f)(__VA_ARGS__);                                     \
    } while (0)

/* ------------------------------------------------------------------------- */
/* XKRT-T callbacks -> OMPT translation                                      */
/* ------------------------------------------------------------------------- */

static void
on_thread_start(runtime_t * runtime, thread_t * thread)
{
    (void) runtime;

    /* An XKOMP master reuses the initial OS thread when it joins a team (a fresh
     * team-local thread_t with tid==0). It already reported thread_begin as the
     * initial thread, so skip its team re-entries. */
    if (thread->team != NULL && thread->tid == 0)
        return ;

    if (!TLS_THREAD_BEGUN)
    {
        const ompt_thread_t type = (thread->team == NULL) ? ompt_thread_initial
                                                          : ompt_thread_worker;
        OMPT_CALL(ompt_callback_thread_begin, ompt_callback_thread_begin_t,
                  type, &TLS_THREAD_DATA);
        TLS_THREAD_BEGUN = true;

        /* the initial thread runs an initial (implicit) task outside any region */
        if (type == ompt_thread_initial)
        {
            OMPT_CALL(ompt_callback_implicit_task, ompt_callback_implicit_task_t,
                      ompt_scope_begin, NULL, as_ompt(&thread->implicit_task.tool_data),
                      1, 0, ompt_task_initial);
            TLS_INITIAL_TASK = true;
        }
    }
}

static void
on_thread_stop(runtime_t * runtime, thread_t * thread)
{
    (void) runtime;

    if (thread->team != NULL && thread->tid == 0)
        return ; /* master team re-entry, not an OS-thread end */

    if (TLS_THREAD_BEGUN)
    {
        OMPT_CALL(ompt_callback_thread_end, ompt_callback_thread_end_t, &TLS_THREAD_DATA);
        TLS_THREAD_BEGUN = false;
    }
}

static void
on_task_create(runtime_t * runtime, task_t * task)
{
    (void) runtime;

    int flags = ompt_task_explicit;
    if (task->flags & TASK_FLAG_UNDEFERABLE)
        flags |= ompt_task_undeferred;

    const int has_deps = (task->flags & TASK_FLAG_ACCESSES) ? 1 : 0;

    ompt_data_t * parent = task->parent ? as_ompt(&task->parent->tool_data) : NULL;
    ompt_data_t * child  = as_ompt(&task->tool_data);
    child->ptr = NULL; /* fresh datum for the tool */

    OMPT_CALL(ompt_callback_task_create, ompt_callback_task_create_t,
              parent, /* encountering_task_frame */ NULL, child, flags, has_deps,
              /* codeptr_ra */ NULL);
}

static void
on_task_schedule(runtime_t * runtime, thread_t * thread, task_t * prev, task_t * next)
{
    (void) runtime; (void) thread;

    OMPT_CALL(ompt_callback_task_schedule, ompt_callback_task_schedule_t,
              prev ? as_ompt(&prev->tool_data) : NULL,
              ompt_task_switch,
              next ? as_ompt(&next->tool_data) : NULL);
}

static void
on_task_complete(runtime_t * runtime, task_t * task)
{
    (void) runtime;

    OMPT_CALL(ompt_callback_task_schedule, ompt_callback_task_schedule_t,
              as_ompt(&task->tool_data), ompt_task_complete, NULL);
}

static ompt_dependence_type_t
xkrt_access_to_ompt_dependence(const access_t * a)
{
    const bool r = (a->mode & ACCESS_MODE_R) != 0;
    const bool w = (a->mode & ACCESS_MODE_W) != 0;

    if (r && w)
        return ompt_dependence_type_inout;
    if (w)
    {
        if (a->concurrency == ACCESS_CONCURRENCY_COMMUTATIVE)
            return ompt_dependence_type_mutexinoutset;
        if (a->concurrency == ACCESS_CONCURRENCY_CONCURRENT)
            return ompt_dependence_type_inoutset;
        return ompt_dependence_type_out;
    }
    return ompt_dependence_type_in;
}

static const void *
xkrt_access_address(const access_t * a)
{
    switch (a->type)
    {
        case ACCESS_TYPE_HANDLE:      return a->region.point.handle;
        case ACCESS_TYPE_SEGMENT:     return (const void *) a->region.interval.segment[0].a;
        case ACCESS_TYPE_BLAS_MATRIX: return (const void *) a->region.matrix.rects[0][0].a;
        default:                      return NULL;
    }
}

static void
on_task_accesses(runtime_t * runtime, task_t * task, const access_t * accesses, xkrt_task_access_counter_t n)
{
    (void) runtime;

    if (OMPT_CB[ompt_callback_dependences] == NULL || n == 0)
        return ;

    ompt_dependence_t * deps = (ompt_dependence_t *) malloc(sizeof(ompt_dependence_t) * n);
    if (deps == NULL)
        return ;

    for (xkrt_task_access_counter_t i = 0 ; i < n ; ++i)
    {
        deps[i].variable.ptr    = (void *) xkrt_access_address(accesses + i);
        deps[i].dependence_type = xkrt_access_to_ompt_dependence(accesses + i);
    }

    OMPT_CALL(ompt_callback_dependences, ompt_callback_dependences_t,
              as_ompt(&task->tool_data), deps, (int) n);

    free(deps);
}

/* emit sync_region + sync_region_wait around a synchronization region */
static inline void
emit_sync_region(ompt_sync_region_t kind, xkrt_scope_t scope,
                 ompt_data_t * parallel_data, ompt_data_t * task_data)
{
    const ompt_scope_endpoint_t endpoint =
        (scope == XKRT_SCOPE_BEGIN) ? ompt_scope_begin : ompt_scope_end;

    if (scope == XKRT_SCOPE_BEGIN)
    {
        OMPT_CALL(ompt_callback_sync_region, ompt_callback_sync_region_t,
                  kind, endpoint, parallel_data, task_data, NULL);
        OMPT_CALL(ompt_callback_sync_region_wait, ompt_callback_sync_region_t,
                  kind, endpoint, parallel_data, task_data, NULL);
    }
    else
    {
        OMPT_CALL(ompt_callback_sync_region_wait, ompt_callback_sync_region_t,
                  kind, endpoint, parallel_data, task_data, NULL);
        OMPT_CALL(ompt_callback_sync_region, ompt_callback_sync_region_t,
                  kind, endpoint, parallel_data, task_data, NULL);
    }
}

static void
on_barrier(runtime_t * runtime, team_t * team, thread_t * thread, xkrt_scope_t scope)
{
    (void) runtime;
    /* XKRT cannot tell apart the various OpenMP barrier kinds (implicit parallel
     * / workshare / explicit), same limitation as libgomp: report a generic
     * implicit-parallel barrier. */
    ompt_data_t * parallel_data = team ? as_ompt(&team->tool_data) : NULL;
    ompt_data_t * task_data     = (thread && thread->current_task)
                                ? as_ompt(&thread->current_task->tool_data) : NULL;
    emit_sync_region(ompt_sync_region_barrier_implicit_parallel, scope, parallel_data, task_data);
}

static void
on_taskwait(runtime_t * runtime, thread_t * thread, task_t * task, xkrt_scope_t scope)
{
    (void) runtime;
    ompt_data_t * parallel_data = (thread && thread->team) ? as_ompt(&thread->team->tool_data) : NULL;
    ompt_data_t * task_data     = task ? as_ompt(&task->tool_data) : NULL;
    emit_sync_region(ompt_sync_region_taskwait, scope, parallel_data, task_data);
}

static void
on_taskgroup(runtime_t * runtime, thread_t * thread, task_t * task, xkrt_scope_t scope)
{
    (void) runtime;
    ompt_data_t * parallel_data = (thread && thread->team) ? as_ompt(&thread->team->tool_data) : NULL;
    ompt_data_t * task_data     = task ? as_ompt(&task->tool_data) : NULL;
    emit_sync_region(ompt_sync_region_taskgroup, scope, parallel_data, task_data);
}

/* ------------------------------------------------------------------------- */
/* construct-level events emitted directly by XKOMP                          */
/* ------------------------------------------------------------------------- */

void
xkomp_ompt_parallel_begin(team_t * team, thread_t * encountering, unsigned int nthreads, const void * codeptr)
{
    if (!OMPT_ENABLED)
        return ;

    ompt_data_t * parallel_data = as_ompt(&team->tool_data);
    parallel_data->ptr = NULL; /* fresh datum for this region (teams are reused) */

    ompt_data_t * enc_task = (encountering && encountering->current_task)
                           ? as_ompt(&encountering->current_task->tool_data) : NULL;

    OMPT_CALL(ompt_callback_parallel_begin, ompt_callback_parallel_begin_t,
              enc_task, /* encountering_task_frame */ NULL, parallel_data,
              nthreads, ompt_parallel_team, codeptr);
}

void
xkomp_ompt_parallel_end(team_t * team, thread_t * encountering, const void * codeptr)
{
    if (!OMPT_ENABLED)
        return ;

    ompt_data_t * parallel_data = as_ompt(&team->tool_data);
    ompt_data_t * enc_task = (encountering && encountering->current_task)
                           ? as_ompt(&encountering->current_task->tool_data) : NULL;

    OMPT_CALL(ompt_callback_parallel_end, ompt_callback_parallel_end_t,
              parallel_data, enc_task, ompt_parallel_team, codeptr);
}

void
xkomp_ompt_implicit_task_begin(thread_t * thread)
{
    if (!OMPT_ENABLED)
        return ;

    team_t * team = thread->team;
    ompt_data_t * parallel_data = team ? as_ompt(&team->tool_data) : NULL;
    ompt_data_t * task_data     = as_ompt(&thread->implicit_task.tool_data);
    task_data->ptr = NULL; /* fresh datum for this region */

    const unsigned int team_size = team ? (unsigned int) team->priv.nthreads : 1;

    OMPT_CALL(ompt_callback_implicit_task, ompt_callback_implicit_task_t,
              ompt_scope_begin, parallel_data, task_data,
              team_size, (unsigned int) thread->tid, ompt_task_implicit);
}

void
xkomp_ompt_implicit_task_end(thread_t * thread)
{
    if (!OMPT_ENABLED)
        return ;

    ompt_data_t * task_data = as_ompt(&thread->implicit_task.tool_data);

    OMPT_CALL(ompt_callback_implicit_task, ompt_callback_implicit_task_t,
              ompt_scope_end, /* parallel_data */ NULL, task_data,
              0, (unsigned int) thread->tid, ompt_task_implicit);
}

/* ------------------------------------------------------------------------- */
/* OMPT runtime entry points (resolved by the tool through the lookup)       */
/* ------------------------------------------------------------------------- */

static bool
ompt_event_supported(ompt_callbacks_t e)
{
    switch (e)
    {
        case ompt_callback_thread_begin:
        case ompt_callback_thread_end:
        case ompt_callback_parallel_begin:
        case ompt_callback_parallel_end:
        case ompt_callback_implicit_task:
        case ompt_callback_task_create:
        case ompt_callback_task_schedule:
        case ompt_callback_dependences:
        case ompt_callback_sync_region:
        case ompt_callback_sync_region_wait:
            return true;
        default:
            return false;
    }
}

static ompt_set_result_t
xkomp_ompt_set_callback(ompt_callbacks_t which, ompt_callback_t callback)
{
    if (which <= 0 || which >= XKOMP_OMPT_CB_MAX)
        return ompt_set_error;
    OMPT_CB[which] = callback;
    if (callback == NULL)
        return ompt_set_always;
    return ompt_event_supported(which) ? ompt_set_always : ompt_set_never;
}

static int
xkomp_ompt_get_callback(ompt_callbacks_t which, ompt_callback_t * callback)
{
    if (which <= 0 || which >= XKOMP_OMPT_CB_MAX || callback == NULL)
        return 0;
    if (OMPT_CB[which] == NULL)
        return 0;
    *callback = OMPT_CB[which];
    return 1;
}

static ompt_data_t *
xkomp_ompt_get_thread_data(void)
{
    return &TLS_THREAD_DATA;
}

static uint64_t
xkomp_ompt_get_unique_id(void)
{
    return OMPT_UID.fetch_add(1, std::memory_order_relaxed);
}

static int
xkomp_ompt_get_parallel_info(int ancestor_level, ompt_data_t ** parallel_data, int * team_size)
{
    if (ancestor_level != 0)
        return 0;
    thread_t * thread = thread_t::get_tls();
    if (thread == NULL || thread->team == NULL)
        return 0;
    if (parallel_data)
        *parallel_data = as_ompt(&thread->team->tool_data);
    if (team_size)
        *team_size = thread->team->priv.nthreads;
    return 2; /* information available and up to date */
}

static int
xkomp_ompt_get_task_info(int ancestor_level, int * flags, ompt_data_t ** task_data,
                         ompt_frame_t ** task_frame, ompt_data_t ** parallel_data, int * thread_num)
{
    if (ancestor_level != 0)
        return 0;
    thread_t * thread = thread_t::get_tls();
    if (thread == NULL || thread->current_task == NULL)
        return 0;
    if (task_data)
        *task_data = as_ompt(&thread->current_task->tool_data);
    if (task_frame)
        *task_frame = NULL;
    if (parallel_data)
        *parallel_data = thread->team ? as_ompt(&thread->team->tool_data) : NULL;
    if (thread_num)
        *thread_num = thread->tid;
    if (flags)
        *flags = (thread->current_task == &thread->implicit_task) ? ompt_task_implicit : ompt_task_explicit;
    return 2;
}

static int
xkomp_ompt_get_task_memory(void ** addr, size_t * size, int block)
{
    (void) block;
    if (addr) *addr = NULL;
    if (size) *size = 0;
    return 0;
}

/* minimal thread-state enumeration */
static const struct { int id; const char * name; } XKOMP_OMPT_STATES[] = {
    { ompt_state_undefined,      "undefined" },
    { ompt_state_work_serial,    "work_serial" },
    { ompt_state_work_parallel,  "work_parallel" },
    { ompt_state_overhead,       "overhead" },
    { ompt_state_wait_barrier,   "wait_barrier" },
    { ompt_state_wait_taskwait,  "wait_taskwait" },
    { ompt_state_wait_taskgroup, "wait_taskgroup" },
    { ompt_state_idle,           "idle" },
};

static int
xkomp_ompt_enumerate_states(int current_state, int * next_state, const char ** next_state_name)
{
    const int len = (int) (sizeof(XKOMP_OMPT_STATES) / sizeof(XKOMP_OMPT_STATES[0]));
    for (int i = 0 ; i < len - 1 ; ++i)
    {
        if (XKOMP_OMPT_STATES[i].id == current_state)
        {
            *next_state      = XKOMP_OMPT_STATES[i + 1].id;
            *next_state_name = XKOMP_OMPT_STATES[i + 1].name;
            return 1;
        }
    }
    return 0;
}

static int
xkomp_ompt_enumerate_mutex_impls(int current_impl, int * next_impl, const char ** next_impl_name)
{
    static const struct { int id; const char * name; } IMPLS[] = {
        { kmp_mutex_impl_none, "none" },
        { kmp_mutex_impl_spin, "spin" },
    };
    const int len = (int) (sizeof(IMPLS) / sizeof(IMPLS[0]));
    for (int i = 0 ; i < len - 1 ; ++i)
    {
        if (IMPLS[i].id == current_impl)
        {
            *next_impl      = IMPLS[i + 1].id;
            *next_impl_name = IMPLS[i + 1].name;
            return 1;
        }
    }
    return 0;
}

static int
xkomp_ompt_get_state(ompt_wait_id_t * wait_id)
{
    if (wait_id)
        *wait_id = 0;
    return ompt_state_work_serial;
}

static int xkomp_ompt_get_num_procs(void)   { long n = sysconf(_SC_NPROCESSORS_ONLN); return n > 0 ? (int) n : 1; }
static int xkomp_ompt_get_num_places(void)  { return 0; }
static int xkomp_ompt_get_place_proc_ids(int, int, int *) { return 0; }
static int xkomp_ompt_get_place_num(void)   { return -1; }
static int xkomp_ompt_get_partition_place_nums(int, int *) { return 0; }
static int xkomp_ompt_get_proc_id(void)     { return sched_getcpu(); }
static int xkomp_ompt_get_num_devices(void) { return 1; }
static int xkomp_ompt_get_target_info(uint64_t *, ompt_id_t *, ompt_id_t *) { return 0; }

/* forward decls of the finalize path (used by ompt_finalize_tool) */
static void xkomp_ompt_do_finalize(void);

static void
xkomp_ompt_finalize_tool(void)
{
    xkomp_ompt_do_finalize();
}

static ompt_interface_fn_t
xkomp_ompt_lookup(const char * name)
{
    if (name == NULL)
        return NULL;

    # define LOOKUP(fn_name, fn) \
        if (strcmp(name, fn_name) == 0) return (ompt_interface_fn_t) fn

    LOOKUP("ompt_set_callback",            xkomp_ompt_set_callback);
    LOOKUP("ompt_get_callback",            xkomp_ompt_get_callback);
    LOOKUP("ompt_get_thread_data",         xkomp_ompt_get_thread_data);
    LOOKUP("ompt_get_unique_id",           xkomp_ompt_get_unique_id);
    LOOKUP("ompt_get_parallel_info",       xkomp_ompt_get_parallel_info);
    LOOKUP("ompt_get_task_info",           xkomp_ompt_get_task_info);
    LOOKUP("ompt_get_task_memory",         xkomp_ompt_get_task_memory);
    LOOKUP("ompt_enumerate_states",        xkomp_ompt_enumerate_states);
    LOOKUP("ompt_enumerate_mutex_impls",   xkomp_ompt_enumerate_mutex_impls);
    LOOKUP("ompt_get_state",               xkomp_ompt_get_state);
    LOOKUP("ompt_get_num_procs",           xkomp_ompt_get_num_procs);
    LOOKUP("ompt_get_num_places",          xkomp_ompt_get_num_places);
    LOOKUP("ompt_get_place_proc_ids",      xkomp_ompt_get_place_proc_ids);
    LOOKUP("ompt_get_place_num",           xkomp_ompt_get_place_num);
    LOOKUP("ompt_get_partition_place_nums", xkomp_ompt_get_partition_place_nums);
    LOOKUP("ompt_get_proc_id",             xkomp_ompt_get_proc_id);
    LOOKUP("ompt_get_num_devices",         xkomp_ompt_get_num_devices);
    LOOKUP("ompt_get_target_info",         xkomp_ompt_get_target_info);
    LOOKUP("ompt_finalize_tool",           xkomp_ompt_finalize_tool);

    # undef LOOKUP

    return NULL;
}

/* ------------------------------------------------------------------------- */
/* discovery                                                                 */
/* ------------------------------------------------------------------------- */

static ompt_start_tool_result_t *
xkomp_ompt_discover(void)
{
    /* OMP_TOOL=disabled disables tooling entirely */
    const char * omp_tool = getenv("OMP_TOOL");
    if (omp_tool && strcmp(omp_tool, "disabled") == 0)
        return NULL;

    /* 1. a tool present in the current address space (linked / LD_PRELOAD'd) */
    if (ompt_start_tool)
    {
        ompt_start_tool_result_t * r = ompt_start_tool(XKOMP_OMPT_VERSION, "xkomp");
        if (r)
            return r;
    }

    /* 2. OMP_TOOL_LIBRARIES: a colon-separated list of tool shared libraries */
    const char * tool_libs = getenv("OMP_TOOL_LIBRARIES");
    if (tool_libs == NULL || tool_libs[0] == '\0')
        return NULL;

    char * libs = strdup(tool_libs);
    if (libs == NULL)
        return NULL;

    ompt_start_tool_result_t * result = NULL;
    char * save = NULL;
    for (char * path = strtok_r(libs, ":", &save) ; path ; path = strtok_r(NULL, ":", &save))
    {
        void * h = dlopen(path, RTLD_LAZY);
        if (h == NULL)
        {
            LOGGER_WARN("OMPT: could not dlopen tool '%s': %s", path, dlerror());
            continue ;
        }
        ompt_start_tool_t start = (ompt_start_tool_t) dlsym(h, "ompt_start_tool");
        if (start == NULL)
        {
            LOGGER_WARN("OMPT: tool '%s' has no 'ompt_start_tool' symbol", path);
            dlclose(h);
            continue ;
        }
        ompt_start_tool_result_t * r = start(XKOMP_OMPT_VERSION, "xkomp");
        if (r)
        {
            OMPT_TOOL_DLHANDLE = h;
            result = r;
            break ;
        }
        dlclose(h);
    }
    free(libs);
    return result;
}

/* ------------------------------------------------------------------------- */
/* lifecycle: XKRT-T tool that drives the OMPT tool                          */
/* ------------------------------------------------------------------------- */

static void
xkomp_ompt_do_finalize(void)
{
    static std::atomic<bool> done{false};
    bool expected = false;
    if (!done.compare_exchange_strong(expected, true))
        return ;

    if (!OMPT_ENABLED)
        return ;

    /* balance the initial thread's implicit task + thread lifecycle (XKOMP has
     * no runtime teardown, so we synthesize them here at program exit) */
    thread_t * thread = thread_t::get_tls();
    if (thread && TLS_INITIAL_TASK)
    {
        OMPT_CALL(ompt_callback_implicit_task, ompt_callback_implicit_task_t,
                  ompt_scope_end, NULL, as_ompt(&thread->implicit_task.tool_data),
                  0, 0, ompt_task_initial);
        TLS_INITIAL_TASK = false;
    }
    if (TLS_THREAD_BEGUN)
    {
        OMPT_CALL(ompt_callback_thread_end, ompt_callback_thread_end_t, &TLS_THREAD_DATA);
        TLS_THREAD_BEGUN = false;
    }

    OMPT_ENABLED = false;

    if (OMPT_TOOL && OMPT_TOOL->finalize)
        OMPT_TOOL->finalize(&OMPT_TOOL->tool_data);

    if (OMPT_TOOL_DLHANDLE)
    {
        dlclose(OMPT_TOOL_DLHANDLE);
        OMPT_TOOL_DLHANDLE = NULL;
    }
}

static void
xkomp_ompt_atexit(void)
{
    xkomp_ompt_do_finalize();
}

/* XKRT-T tool initialize: discover + start the OMPT tool, register callbacks */
static int
xkomp_ompt_bridge_initialize(runtime_t * runtime, xkrt_tool_data_t * tool_data)
{
    (void) tool_data;

    OMPT_TOOL = xkomp_ompt_discover();
    if (OMPT_TOOL == NULL)
        return 0; /* no OMPT tool -> keep XKRT-T disabled (zero overhead) */

    int enabled = 1;
    if (OMPT_TOOL->initialize)
        enabled = OMPT_TOOL->initialize(xkomp_ompt_lookup, XKOMP_OMPT_INITIAL_DEVICE, &OMPT_TOOL->tool_data);

    if (!enabled)
    {
        LOGGER_INFO("OMPT: tool declined activation");
        if (OMPT_TOOL_DLHANDLE)
        {
            dlclose(OMPT_TOOL_DLHANDLE);
            OMPT_TOOL_DLHANDLE = NULL;
        }
        OMPT_TOOL = NULL;
        return 0;
    }

    OMPT_ENABLED = true;

    /* register the XKRT-T callbacks the bridge translates to OMPT */
    # define REGISTER(EV, FN) runtime->tool_set_callback(EV, (xkrt_callback_generic_t) FN)
    REGISTER(XKRT_CALLBACK_THREAD_START,   on_thread_start);
    REGISTER(XKRT_CALLBACK_THREAD_STOP,    on_thread_stop);
    REGISTER(XKRT_CALLBACK_TASK_CREATE,    on_task_create);
    REGISTER(XKRT_CALLBACK_TASK_SCHEDULE,  on_task_schedule);
    REGISTER(XKRT_CALLBACK_TASK_COMPLETE,  on_task_complete);
    REGISTER(XKRT_CALLBACK_TASK_ACCESSES,  on_task_accesses);
    REGISTER(XKRT_CALLBACK_BARRIER,        on_barrier);
    REGISTER(XKRT_CALLBACK_TASKWAIT,       on_taskwait);
    REGISTER(XKRT_CALLBACK_TASKGROUP,      on_taskgroup);
    # undef REGISTER

    /* finalize at program exit (XKOMP has no runtime teardown) */
    atexit(xkomp_ompt_atexit);

    LOGGER_INFO("OMPT: tool activated");
    return 1;
}

static void
xkomp_ompt_bridge_finalize(xkrt_tool_data_t * tool_data)
{
    (void) tool_data;
    xkomp_ompt_do_finalize();
}

/* the XKRT-T tool description registered with the runtime */
static xkrt_tool_result_t XKOMP_OMPT_BRIDGE = {
    &xkomp_ompt_bridge_initialize,
    &xkomp_ompt_bridge_finalize,
    { .value = 0 }
};

void
xkomp_ompt_connect(runtime_t * runtime)
{
    runtime->tool_connect(&XKOMP_OMPT_BRIDGE);
}

# endif /* XKRT_SUPPORT_TOOLS && XKOMP_SUPPORT_OMPT */
