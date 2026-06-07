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

/**
 * KMP-versioned entry point for omp_target_memcpy_async.
 * The depobj_list uses KMP's depend representation (kmp_depend_info_t).
 */
extern "C"
int
__kmp_omp_target_memcpy_async(
    void * dst_ptr,
    const void * src_ptr,
    size_t length,
    size_t dst_offset,
    size_t src_offset,
    int dst_device_num,
    int src_device_num,
    int depobj_count,
    void * depobj_list
) {
    uintptr_t dst = ((uintptr_t) dst_ptr) + dst_offset;
    uintptr_t src = ((uintptr_t) src_ptr) + src_offset;

    const kmp_int32 naccesses = depobj_count;
    assert(naccesses <= XKRT_TASK_MAX_ACCESSES);

    thread_t * thread = thread_t::get_tls();
    assert(thread);

    xkomp_t * xkomp = xkomp_get();
    assert(xkomp);

    const xkrt_device_unique_id_t src_device_unique_id = omp_device_id_to_xkomp(src_device_num);
    const xkrt_device_unique_id_t dst_device_unique_id = omp_device_id_to_xkomp(dst_device_num);

    const xkrt_device_unique_id_t device_unique_id = src_device_unique_id;

    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DETACHABLE;

    xkomp->runtime.task_spawn<flags>(
        device_unique_id,
        naccesses,
        [&] (task_t * task, access_t * accesses)
        {
            LOGGER_FATAL("TODO");
            // TODO
        },
        nullptr,
        [&] (runtime_t * runtime, device_t * device, task_t * task)
        {
            ocg::command_type_t ctype;
            command_queue_type_t qtype;
            device_t::offloader_command_types(
                length,
                dst_device_unique_id, dst,
                src_device_unique_id, src,
                ctype, qtype
            );

            constexpr command_flag_t flags = COMMAND_FLAG_NONE;

            const auto builder = [&] (command_t * command) {
                command->copy_1D.src_device_unique_id = src_device_unique_id;
                command->copy_1D.dst_device_unique_id = dst_device_unique_id;
                command->copy_1D.src_device_addr      = src;
                command->copy_1D.dst_device_addr      = dst;
                command->copy_1D.size                 = length;
            };
            runtime->task_emit_command(device->unique_id, qtype, ctype, flags, builder);
        }
    );

    return 0;
}
