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

// fill the binding/membership description fields shared by all xkomp teams
static inline void
xkomp_team_desc_init(
    xkomp_t * omp,
    team_t * team,
    unsigned int nthreads
) {
    team->desc.binding.flags       = XKRT_TEAM_BINDING_FLAG_NONE;
    team->desc.binding.mode        = parse_proc_bind(omp->env.OMP_PROC_BIND);
    team->desc.binding.nplaces     = 0;
    team->desc.binding.places      = parse_places(omp->env.OMP_PLACES);
    team->desc.binding.places_list = NULL;
    team->desc.master_is_member    = true;
    team->desc.nthreads            = nthreads;
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

    // Nested parallel region: the calling thread already belongs to a team.
    // Serialize it in the calling thread by running a one-shot team of 1.
    thread_t * tls = thread_t::get_tls();
    if (tls->team != NULL)
    {
        team_t team;
        xkomp_team_desc_init(omp, &team, 1);
        team.desc.args    = args;
        team.desc.routine = (team_routine_t) routine;

        omp->runtime.team_create(&team);
        omp->runtime.team_join(&team);
        return ;
    }

    // Top-level parallel region: reuse a cached persistent team with the same
    // number of threads, or lazily spawn one on first use. Its worker threads
    // stay parked between regions (routine == XKRT_TEAM_ROUTINE_PARALLEL_FOR).
    team_t * team = NULL;
    for (xkomp_team_entry_t & e : omp->teams)
    {
        if (e.nthreads == (int) nthreads)
        {
            team = &e.team;
            break ;
        }
    }

    if (team == NULL)
    {
        // store the team in-place in the small vector (no heap allocation).
        // teams must not move once created (workers hold their address), so we
        // must stay within the vector's inline capacity.
        assert(omp->teams.size() < XKOMP_MAX_CACHED_TEAMS);
        xkomp_team_entry_t * entry = omp->teams.emplace_back();
        entry->nthreads = (int) nthreads;
        team = &entry->team;

        xkomp_team_desc_init(omp, team, nthreads);
        team->desc.args    = NULL;
        team->desc.routine = XKRT_TEAM_ROUTINE_PARALLEL_FOR;

        omp->runtime.team_create(team);
        assert(team->priv.threads != NULL);
        assert(nthreads == 0 || team->priv.nthreads == (int) nthreads);
    }

    // dispatch the region body onto the persistent team and wait for completion.
    // the body's own trailing `team_barrier` (in `routine`) provides the
    // implicit end-of-region barrier and drains any deferred tasks.
    team->desc.args = args;
    omp->runtime.team_parallel_for(team,
        [omp, routine] (thread_t * thread) {
            routine(&omp->runtime, thread->team, thread);
        }
    );
}
