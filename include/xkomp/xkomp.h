#ifndef __XKOMP_H__
# define __XKOMP_H__

# include <xkrt/xkrt.h>
# include <xkomp/support.h>

/* environment variables parsed at program starts */
typedef struct  xkomp_env_t
{
    char    OMP_DISPLAY_ENV;
    int     OMP_NUM_THREADS;
    int     OMP_THREAD_LIMIT;
    char *  OMP_PLACES;
    char *  OMP_PROC_BIND;
}               xkomp_env_t;

/** global variable that holds the entire openmp context */
typedef struct  xkomp_t
{
    /* underlaying XKaapi runtime */
    xkrt_runtime_t runtime;

    /* omp task format */
    task_format_id_t task_format;

    /* environment variables */
    xkomp_env_t env;

    /* the team of thread for parallel region */
    xkrt_team_t team;
}               xkomp_t;

extern xkomp_t * xkomp;

extern "C"
xkomp_t * xkomp_get(void);

/** load env variables */
void xkomp_env_init(xkomp_env_t * env);

/* save task format */
void xkomp_task_register_format(xkomp_t * xkomp);

# define XKOMP_HACK_TARGET_CALL 1
# if XKOMP_HACK_TARGET_CALL
extern "C" {
    task_t                              * xkomp_current_task(void);
    xkrt_stream_t                       * xkomp_current_stream(void);
    xkrt_stream_instruction_t           * xkomp_current_stream_instruction(void);
    xkrt_stream_instruction_counter_t     xkomp_current_stream_instruction_counter(void);
};
# endif /* XKOMP_HACK_TARGET_CALL */

# endif /* __XKOMP_H__ */
