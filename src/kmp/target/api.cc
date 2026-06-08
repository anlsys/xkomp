extern "C" {
# include <xkomp/kmp.h>
};

# include <xkomp/omp.h>
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

extern "C"
void **
__kmpc_omp_get_target_async_handle_ptr(kmp_int32 gtid)
{
    return NULL;
}

extern "C"
void
__kmp_omp_task_deps_to_xkomp_accesses(
    task_t * task,
    access_t * accesses,
    kmp_depend_info_t * dep_list,
    kmp_int32 ndeps
);

typedef struct  xkomp_target_memcpy_args_t
{
    uintptr_t dst;
    uintptr_t src;
    size_t size;
    xkrt_device_unique_id_t src_device_unique_id;
    xkrt_device_unique_id_t dst_device_unique_id;
}               xkomp_target_memcpy_args_t;

/**
 * KMP-versioned entry point for omp_target_memcpy_async.
 * The depobj_list uses KMP's depend representation (kmp_depend_info_t).
 */
extern "C"
int
__kmp_omp_target_memcpy_async(
    void * dst_ptr,
    const void * src_ptr,
    size_t size,
    size_t dst_offset,
    size_t src_offset,
    int dst_device_num,
    int src_device_num,
    int depobj_count,
    void * vdepobj_list
) {
    assert(depobj_count <= XKRT_TASK_MAX_ACCESSES);
    omp_depend_t * depobj_list = (omp_depend_t *) vdepobj_list;

    uintptr_t dst = ((uintptr_t) dst_ptr) + dst_offset;
    uintptr_t src = ((uintptr_t) src_ptr) + src_offset;

    xkomp_t * xkomp = xkomp_get();
    assert(xkomp);

    const xkrt_device_unique_id_t src_device_unique_id = omp_device_id_to_xkomp(src_device_num);
    const xkrt_device_unique_id_t dst_device_unique_id = omp_device_id_to_xkomp(dst_device_num);

    const xkrt_device_unique_id_t device_unique_id = src_device_unique_id;

    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DETACHABLE | TASK_FLAG_ACCESSES;

    const xkomp_target_memcpy_args_t args = {
        .dst = dst,
        .src = src,
        .size = size,
        .src_device_unique_id = src_device_unique_id,
        .dst_device_unique_id = dst_device_unique_id
    };
    constexpr size_t args_size = sizeof(xkomp_target_memcpy_args_t);

    const task_format_id_t fmtid = xkomp->formats.kmp.target_memcpy_async;

    task_t * task = xkomp->runtime.task_instanciate<flags>(
        device_unique_id,
        depobj_count,
        [=] (task_t * task, access_t * accesses)
        {
            # ifndef NDEBUG
            snprintf(task->label, sizeof(task->label), "omp-task-target-memcpy-async");
            # endif /* NDEBUG */
            for (int i = 0 ; i < depobj_count ; ++i)
                __kmp_omp_task_deps_to_xkomp_accesses(task, accesses + i, (kmp_depend_info_t *) depobj_list[i], 1);
        },
        nullptr,
        &args,
        args_size,
        fmtid
    );

    xkomp->runtime.task_commit(task);

    return 0;
}

// called from a device thread
static inline void
body_omp_target_memcpy_async(
    runtime_t * runtime,
    device_t * device,
    task_t * task
) {
    xkomp_target_memcpy_args_t * args = (xkomp_target_memcpy_args_t *) TASK_ARGS(task);
    assert(args);

    ocg::command_type_t ctype;
    command_queue_type_t qtype;
    device_t::offloader_command_types(
        args->size,
        args->dst_device_unique_id, args->dst,
        args->src_device_unique_id, args->src,
        ctype, qtype
    );

    constexpr command_flag_t flags = COMMAND_FLAG_NONE;
    const auto builder = [&] (command_t * command) {
        command->copy_1D.src_device_unique_id = args->src_device_unique_id;
        command->copy_1D.dst_device_unique_id = args->dst_device_unique_id;
        command->copy_1D.src_device_addr      = args->src;
        command->copy_1D.dst_device_addr      = args->dst;
        command->copy_1D.size                 = args->size;
    };
    runtime->task_emit_command(device->unique_id, qtype, ctype, flags, builder);
}

void
xkomp_task_register_formats_kmp_target(xkomp_t * xkomp)
{
    task_format_t format;
    memset(&format, 0, sizeof(task_format_t));
    for (int i = 0 ; i < XKRT_TASK_FORMAT_TARGET_MAX ; ++i)
        format.f[i] = (task_format_func_t) body_omp_target_memcpy_async;
    snprintf(format.label, sizeof(format.label), "omp-task");
    xkomp->formats.kmp.target_memcpy_async = xkomp->runtime.task_format_create(&format);
}
