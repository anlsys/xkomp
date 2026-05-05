# include <xkomp/xkomp.h>
# include <xkomp/kmp.h>

# include <xkrt/logger/logger.h>

# include <assert.h>
# include <stdarg.h>

extern "C"
kmp_int32
__kmpc_global_thread_num(ident_t * loc)
{
    (void) loc;

    // ensure runtime is initialized
    xkomp_get();

    thread_t * tls = thread_t::get_tls();
    assert(tls);

    return tls->gtid;
}
