#ifndef __XKOMP_TASKGRAPH_H__
# define __XKOMP_TASKGRAPH_H__

# include <xkrt/runtime.h>
XKRT_NAMESPACE_USE;

typedef enum    xkomp_taskgraph_flags_t
{
    XKOMP_TASKGRAPH_FLAG_RESET      = (1 << 0),
    XKOMP_TASKGRAPH_FLAG_IF         = (1 << 1),
    XKOMP_TASKGRAPH_FLAG_NOGROUP    = (1 << 2)

}               xkomp_taskgraph_flags_t;

/* graph_id type */
typedef int xkomp_taskgraph_id_t;

/* taskgraph internal structure */
typedef struct  xkomp_taskgraph_t
{
    /* XKRT tdg */
    task_dependency_graph_t tdg;

    /* XKRT cg */
    command_graph_t cg;

    /* graph id */
    xkomp_taskgraph_id_t id;

    /* flags */
    xkomp_taskgraph_flags_t flags;

    /* Replay counter -- count the number of time the pragma is encoutered for that graph */
    int rc;

}               xkomp_taskgraph_t;

extern "C" {

    /**
     *  # pragma omp taskgraph graph_id(int) graph_reset(bool) if(bool) nogroup
     */
    xkomp_taskgraph_t * xkomp_taskgraph_begin(int graph_id, xkomp_taskgraph_flags_t flags);
    void xkomp_taskgraph_end(xkomp_taskgraph_t * taskgraph);


};

#endif /* __XKOMP_TASKGRAPH_H__ */
