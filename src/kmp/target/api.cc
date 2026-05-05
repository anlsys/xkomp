# include <xkomp/xkomp.h>
# include <xkomp/support.h>

# include <xkrt/thread/thread.h>

extern "C"
int
__kmpc_get_target_offload(void)
{
    # if XKOMP_SUPPORT_TARGET
    return 1;
    # else
    return 0;
    # endif
}

/* check if the current thread has a team */
extern "C"
bool
__kmpc_omp_has_task_team(int gtid)
{
    thread_t * tls = thread_t::get_tls();
    assert(tls);
    assert(gtid == tls->gtid);
    return tls->team != NULL;
}
