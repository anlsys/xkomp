#ifndef __XKOMP_PLUS_PLUS_H__
# define __XKOMP_PLUS_PLUS_H__

/**
 *  a C++ API for OpenMP, so we can fastly prototype APIs
 */

# include <xkomp/xkomp.h>
# include <functional>

static inline void
pragma_omp_taskgraph(
    xkomp_taskgraph_id_t graph_id,
    xkomp_taskgraph_flags_t flags,
    std::function<void(void)> f
) {
    xkomp_taskgraph_t * taskgraph = xkomp_taskgraph_begin(graph_id, flags);
    if (taskgraph->rc == 1)
        f();
    xkomp_taskgraph_end(taskgraph);
}

/**
 *  An iterative loop whose body is recorded once as a taskgraph and replayed.
 *
 *  A `taskgraph` instance carries an implicit taskgroup, so instance i+1 cannot
 *  start before i has fully drained. For a loop whose iterations could otherwise
 *  overlap, that barrier is pure loss: it caps the achievable speedup at the
 *  parallelism of ONE iteration, while the same code written as plain dependent
 *  tasks keeps pipelining across iterations. (Measured on LULESH s=100: the plain
 *  task version reaches 2.97x over the synchronous schedule, a per-iteration
 *  taskgraph is stuck at 1.98x.)
 *
 *  This construct moves the loop *inside* the graph: `unroll` consecutive
 *  iterations are recorded as one instance, so they overlap with each other and
 *  the barrier is paid once per `unroll` iterations instead of once per
 *  iteration. The graph stays acyclic and there is still a single instance in
 *  flight, so graph optimizations and replay are unaffected -- only the recorded
 *  body is `unroll` times larger.
 *
 *      graph_id : as for pragma_omp_taskgraph
 *      flags    : as for pragma_omp_taskgraph
 *      unroll   : iterations per recorded instance (>= 1; 1 == pragma_omp_taskgraph
 *                 in a loop)
 *      cond     : bool(size_t done) -- the loop condition, evaluated on the host
 *                 between instances with the number of iterations already issued.
 *                 A fixed trip count is `[n](size_t d){ return d < n; }`; a
 *                 convergence test reads whatever the previous instance produced
 *                 (the instance boundary is a full barrier, so device results
 *                 written back by the graph are visible here).
 *      epilogue : void(size_t inst, size_t done) -- host code run after each
 *                 instance, OUTSIDE the recorded region. This is where
 *                 per-iteration timing / progress / convergence bookkeeping
 *                 belongs. It may spawn tasks: keeping such work asynchronous is
 *                 what stops it from re-introducing a barrier in the
 *                 configurations that do not use a taskgraph.
 *      body     : void(void) -- ONE iteration.
 *
 *  Returns the number of iterations issued (always a multiple of `unroll`).
 *
 *  Two properties of `body` are load-bearing:
 *
 *   - It is invoked only while recording, so everything it captures is frozen
 *     into the graph and re-executed verbatim on every replay. It therefore takes
 *     NO iteration index: an index baked into a recorded task (a `firstprivate`
 *     loop counter, an index-derived address) would be wrong from the second
 *     instance on. Iteration-varying state must live in the data the graph reads
 *     and writes -- a device-side counter, a 1-element buffer chained through
 *     `depend` -- not in the host closure.
 *   - It must produce the same tasks, on the same addresses, every time.
 *
 *  Termination is evaluated at instance granularity, so a data-dependent loop may
 *  overshoot by up to `unroll - 1` iterations. Use pragma_omp_taskgraphloop_n for
 *  an exact trip count.
 *
 *  This is built on xkomp_taskgraph_begin/end: the loop itself stays on the host
 *  and only the unrolled body is recorded, so the host still runs once per
 *  instance and the instance boundary is still a barrier. The runtime-level
 *  xkomp_taskgraphloop_begin/end (xkomp.h, currently stubs) is the further step:
 *  it carries the loop condition as an event handle, so the whole loop can be one
 *  graph -- a CUDA conditional node -- and the host drops out entirely. Until
 *  then, `unroll` is what amortizes the barrier.
 */
template <typename Cond, typename Epilogue, typename Body>
static inline size_t
pragma_omp_taskgraphloop(
    xkomp_taskgraph_id_t graph_id,
    xkomp_taskgraph_flags_t flags,
    size_t unroll,
    Cond && cond,
    Epilogue && epilogue,
    Body && body
) {
    if (unroll == 0)
        unroll = 1;

    size_t done = 0;
    size_t inst = 0;

    while (cond(done))
    {
        xkomp_taskgraph_t * taskgraph = xkomp_taskgraph_begin(graph_id, flags);
        if (taskgraph->rc == 1)
            for (size_t u = 0 ; u < unroll ; ++u)
                body();
        xkomp_taskgraph_end(taskgraph);

        done += unroll;
        epilogue(inst++, done);
    }

    return done;
}

/**
 *  pragma_omp_taskgraphloop with an exact trip count `n`: floor(n / unroll)
 *  recorded instances, then the remaining `n % unroll` iterations as plain
 *  (unrecorded) tasks, so the loop runs exactly `n` times with no overshoot. The
 *  remainder cannot be replayed -- a shorter instance is a different graph -- so
 *  prefer an `n` that is a multiple of `unroll` when it is the measured region.
 */
template <typename Epilogue, typename Body>
static inline size_t
pragma_omp_taskgraphloop_n(
    xkomp_taskgraph_id_t graph_id,
    xkomp_taskgraph_flags_t flags,
    size_t unroll,
    size_t n,
    Epilogue && epilogue,
    Body && body
) {
    if (unroll == 0)
        unroll = 1;

    const size_t whole = (n / unroll) * unroll;
    size_t done = pragma_omp_taskgraphloop(graph_id, flags, unroll,
                        [whole] (size_t d) { return d < whole; }, epilogue, body);

    for ( ; done < n ; ++done)
        body();

    return done;
}

#endif /* __XKOMP_PLUS_PLUS_H__ */
