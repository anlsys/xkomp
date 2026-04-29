# include <xkomp/xkomp.h>

extern "C" {
    # include <xkomp/kmp.h>
};

# include <xkrt/logger/logger.h>

# include <assert.h>
# include <stdarg.h>

typedef struct  wargs_t
{
    int argc;
    void * args[XKOMP_MICROTASK_MAX_ARGS];
    kmpc_micro f;
}               wargs_t;

static void *
fork_call_wrapper(
    runtime_t * runtime,
    team_t * team,
    thread_t * thread
) {
    assert(thread);

    wargs_t * wargs = (wargs_t *) thread->team->desc.args;
	__kmp_invoke_microtask(wargs->f, thread->gtid, thread->tid, wargs->argc, wargs->args);

    xkomp_t * omp = xkomp_get();
    omp->runtime.team_barrier<true>(thread->team, thread);

    return NULL;
}

_Thread_local int pushed_num_threads;

// # pragma omp [...] num_threads(X)
extern "C"
void
__kmpc_push_num_threads(
    ident_t * loc,
    kmp_int32 global_tid,
    kmp_int32 num_threads
) {
    pushed_num_threads = num_threads < 0 ? 0 : num_threads;
}

static team_binding_places_t
parse_places(
    const char * places
) {
    if (places == NULL)
        return XKRT_TEAM_BINDING_PLACES_HYPERTHREAD;

    struct mapping_struct_s {
        const char * name;
        team_binding_places_t places;
    };

    constexpr struct mapping_struct_s mapping[] = {
        {  "threads",   XKRT_TEAM_BINDING_PLACES_HYPERTHREAD},
        {  "cores",     XKRT_TEAM_BINDING_PLACES_CORE       },
        {  "L1s",       XKRT_TEAM_BINDING_PLACES_L1         },
        {  "L2s",       XKRT_TEAM_BINDING_PLACES_L2         },
        {  "L3s",       XKRT_TEAM_BINDING_PLACES_L3         },
        {  "numas",     XKRT_TEAM_BINDING_PLACES_NUMA       },
        {  "devices",   XKRT_TEAM_BINDING_PLACES_DEVICE     },
        {  "sockets",   XKRT_TEAM_BINDING_PLACES_SOCKET     },
        {  "machines",  XKRT_TEAM_BINDING_PLACES_MACHINE    }
    };

    constexpr unsigned int nmapping = sizeof(mapping) / sizeof(struct mapping_struct_s);

    for (unsigned int i = 0 ; i < nmapping ; ++i)
        if (strcmp(mapping[i].name, places) == 0)
            return mapping[i].places;

    constexpr const char * values = "threads, cores, L1s, L2s, L3s, numas, devices, sockets, machines";
    constexpr unsigned int fb = 1;
    LOGGER_WARN("Unknown `OMP_PLACES=%s` - falling back to %s. Available values are %s",
            places, mapping[fb].name, values);

    return mapping[fb].places;
}

static team_binding_mode_t
parse_proc_bind(
    const char * proc_bind
) {
    if (proc_bind)
    {
        if (strcmp(proc_bind, "close") == 0)
            return XKRT_TEAM_BINDING_MODE_COMPACT;
        if (strcmp(proc_bind, "spread") == 0)
            return XKRT_TEAM_BINDING_MODE_SPREAD;
    }
    return XKRT_TEAM_BINDING_MODE_COMPACT;
}

// # pragma omp parallel
extern "C"
void
__kmpc_fork_call(
    ident_t * loc,
    kmp_int32 argc,
    kmpc_micro f,
    ...
) {
    assert(argc <= XKOMP_MICROTASK_MAX_ARGS);

    // copy parallel region routine arguments
    wargs_t * wargs = (wargs_t *) malloc(sizeof(wargs_t));
    assert(wargs);
    va_list args;
    va_start(args, f);
    wargs->argc = argc;
    wargs->f = f;
    for (kmp_int32 i = 0; i < argc; ++i)
        wargs->args[i] = va_arg(args, void *);
    va_end(args);

    // run xkomp parallel
    xkomp_parallel(pushed_num_threads, (team_routine_t) fork_call_wrapper, wargs);
    free(wargs);

    // reset pushed num threads
    pushed_num_threads = 0;
}

extern "C"
void
__kmpc_fork_teams(
    ident_t * loc,
    kmp_int32 argc,
    kmpc_micro microtask,
    ...
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    LOGGER_NOT_IMPLEMENTED();
    # if 0
    va_list ap;
    va_start(ap, microtask);

    // remember teams entry point and nesting level
    this_thr->th.th_teams_microtask = microtask;
    this_thr->th.th_teams_level = this_thr->th.th_team->t.t_level; // AC: can be >0 on host

    // check if __kmpc_push_num_teams called, set default number of teams otherwise
    if (this_thr->th.th_teams_size.nteams == 0)
        __kmp_push_num_teams(loc, gtid, 0, 0);

    KMP_DEBUG_ASSERT(this_thr->th.th_set_nproc >= 1);
    KMP_DEBUG_ASSERT(this_thr->th.th_teams_size.nteams >= 1);
    KMP_DEBUG_ASSERT(this_thr->th.th_teams_size.nth >= 1);

    __kmp_fork_call(
            loc, gtid, fork_context_intel, argc,
            VOLATILE_CAST(microtask_t) __kmp_teams_master, // "wrapped" task
            VOLATILE_CAST(launch_t) __kmp_invoke_teams_master, kmp_va_addr_of(ap));
    __kmp_join_call(loc, gtid);

    // Pop current CG root off list
    KMP_DEBUG_ASSERT(this_thr->th.th_cg_roots);
    kmp_cg_root_t *tmp = this_thr->th.th_cg_roots;
    this_thr->th.th_cg_roots = tmp->up;
    KMP_DEBUG_ASSERT(tmp->cg_nthreads);
    int i = tmp->cg_nthreads--;
    if (i == 1) { // check is we are the last thread in CG (not always the case)
        __kmp_free(tmp);
    }
    // Restore current task's thread_limit from CG root
    KMP_DEBUG_ASSERT(this_thr->th.th_cg_roots);
    this_thr->th.th_current_task->td_icvs.thread_limit =
        this_thr->th.th_cg_roots->cg_thread_limit;

    this_thr->th.th_teams_microtask = NULL;
    this_thr->th.th_teams_level = 0;
    memset(&this_thr->th.th_teams_size, 0, sizeof(kmp_teams_size_t));
    va_end(ap);
    # endif
}
