# include <xkomp/xkomp.h>

extern "C"
xkomp_taskgraph_t *
xkomp_taskgraph_begin(
    int graph_id,
    xkomp_taskgraph_flags_t flags
) {
    // clauses are not supported yet
    if (flags & XKOMP_TASKGRAPH_FLAG_RESET)
        LOGGER_FATAL("Not supported");

    if (flags & XKOMP_TASKGRAPH_FLAG_IF)
        LOGGER_FATAL("Not supported");

    if (flags & XKOMP_TASKGRAPH_FLAG_NOGROUP)
        LOGGER_FATAL("Not supported");

    // retrieve tdg
    xkomp_t * xkomp = xkomp_get();
    xkomp_taskgraph_t & taskgraph = xkomp->taskgraphs[graph_id];
    ++taskgraph.rc;

    // first execution = record
    if (taskgraph.rc == 1)
    {
        xkomp->runtime.task_dependency_graph_record_start(&taskgraph.tdg);
    }
    else
    {
        // second or further executions

        // optimize on first replay
        if (taskgraph.rc == 2)
        {
            /* build a CG from a tdg */
            xkomp->runtime.command_graph_from_task_dependency_graph(&taskgraph.tdg, false, &taskgraph.cg);

            /* remove useless nodes */
            //  # pragma omp taskgraph optimize(reduce_nodes)
            taskgraph.cg.optimize(COMMAND_GRAPH_OPT_REDUCE_NODE);

            /* remove redundant edges */
            //  # pragma omp taskgraph optimize(reduce_edges)
            taskgraph.cg.optimize(COMMAND_GRAPH_OPT_REDUCE_EDGE);

            /* contract the CG */
            //  # pragma omp taskgraph optimize(batch)
            taskgraph.cg.optimize(COMMAND_GRAPH_OPT_BATCH);
        }

        /* replay the CG */
        xkomp->runtime.command_graph_replay(&taskgraph.cg);
    }

    // TODO: if map is resized, address would change
    return &taskgraph;
}

extern "C"
void
xkomp_taskgraph_end(xkomp_taskgraph_t * taskgraph)
{
    xkomp_t * xkomp = xkomp_get();

    // stop recording
    if (taskgraph->rc == 1)
    {
        // implicit taskwait in XKRT
        xkomp->runtime.task_dependency_graph_record_stop();
    }
    else
    {
        // TODO
    }
}
