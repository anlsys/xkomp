# include <stdlib.h>
# include <xkomp/xkomp.h>

extern "C" {
    # include <xkomp/kmp.h>
};

typedef void * omp_allocator_handle_t;

extern "C"
void *
__kmpc_alloc(
    int gtid,
    size_t sz,
    omp_allocator_handle_t al
) {
    LOGGER_DEBUG("__kmpc_alloc hacked");
    return malloc(sz);
}

extern "C"
void
__kmpc_free(
    int gtid,
    void * ptr,
    omp_allocator_handle_t al
) {
    LOGGER_DEBUG("__kmpc_free hacked");
    return free(ptr);
}
