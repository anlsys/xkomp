# include <xkomp/xkomp.h>
# include <xkrt/logger/logger.h>

# include <assert.h>
# include <stdarg.h>

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
xkomp_parallel(
    unsigned int nthreads,
    team_routine_t routine,
    void * args
) {
    // parse number of threads - see Algorithm 12.1 Determine Number of Threads
    // This is not standard, but whatever for now
    xkomp_t * omp = xkomp_get();
    nthreads = nthreads ? nthreads : omp->env.OMP_NUM_THREADS ? omp->env.OMP_NUM_THREADS : 0;
    if (nthreads > omp->env.OMP_THREAD_LIMIT)
        nthreads = omp->env.OMP_THREAD_LIMIT;

    // create the team
    team_t team;
    team.desc.args                = args;
    team.desc.binding.flags       = XKRT_TEAM_BINDING_FLAG_NONE;
    team.desc.binding.mode        = parse_proc_bind(omp->env.OMP_PROC_BIND);
    team.desc.binding.nplaces     = 0;
    team.desc.binding.places      = parse_places(omp->env.OMP_PLACES);
    team.desc.binding.places_list = NULL;
    team.desc.master_is_member    = true;
    team.desc.nthreads            = nthreads;
    team.desc.routine             = (team_routine_t) routine;

    // spawn the team
    omp->runtime.team_create(&team);
    assert(team.priv.threads != NULL);
    assert(nthreads == 0 || team.priv.nthreads == nthreads);

    // wait for completion
    omp->runtime.team_join(&team);
}
