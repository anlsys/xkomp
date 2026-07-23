# include <xkomp/xkomp.h>
# include <xkomp/kmp.h>

# include <xkrt/logger/logger.h>

# include <assert.h>
# include <stdarg.h>

extern "C"
int32_t
__kmpc_master(ident_t * loc, int32_t gtid)
{
    thread_t * tls = thread_t::get_tls();
    assert(tls);

    // TODO: should instead check that this is in-deed the team master
    return tls->tid == 0;
}

extern "C"
void
__kmpc_end_master(ident_t *loc, kmp_int32 global_tid)
{

}

// `masked` (OpenMP 5.1) generalizes `master`: the region is executed by the
// single thread whose team-local id equals `filter` (the value of the `filter`
// clause, or 0 -- i.e. the primary thread -- when the clause is absent).
// Returns 1 on that thread and 0 on the others; unlike `single`, no thread waits
// here (a following `barrier` is what the others synchronize on).
extern "C"
int32_t
__kmpc_masked(ident_t * loc, int32_t gtid, int32_t filter)
{
    thread_t * tls = thread_t::get_tls();
    assert(tls);

    return tls->tid == filter;
}

extern "C"
void
__kmpc_end_masked(ident_t * loc, kmp_int32 global_tid)
{

}
