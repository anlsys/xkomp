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
