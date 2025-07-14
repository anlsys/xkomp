# include <xkomp/xkomp.h>
# include <xkomp/kmp.h>

# include <xkrt/logger/logger.h>

# include <assert.h>

// see llvm impl
const size_t
round_up_to_val(size_t size, size_t val)
{
    if (size & (val - 1))
    {
        size &= ~(val - 1);
        if (size <= SIZE_MAX - val)
        {
            size += val;
        }
    }
    return size;
}

constexpr int AC = 0;
constexpr task_flag_bitfield_t xkflags = TASK_FLAG_ZERO;
constexpr size_t task_size      = task_compute_size(xkflags, AC);

extern "C"
kmp_task_t *
__kmpc_omp_task_alloc(
    ident_t * loc_ref,
    kmp_int32 gtid,
    kmp_int32 flags,
    size_t sizeof_kmp_task_t,
    size_t sizeof_shareds,
    kmp_routine_entry_t task_entry
) {
    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    xkomp_t * xkomp = xkomp_get();
    assert(xkomp);

    // see llvm impl
    const size_t args_size      = sizeof_kmp_task_t;
    const size_t total_size     = task_size + args_size;
    const size_t shareds_offset = round_up_to_val(total_size, sizeof(void *));
    assert(shareds_offset >= total_size);
    assert(shareds_offset % sizeof(void *) == 0);

    task_t * task = thread->allocate_task(shareds_offset + sizeof_shareds);
    new(task) task_t(xkomp->task_format, xkflags);

    kmp_task_t * ktask = (kmp_task_t *) TASK_ARGS(task, task_size);
    assert(ktask);

    ktask->shareds = (sizeof_shareds > 0) ? ((char *) task) + shareds_offset : NULL;
    ktask->routine = task_entry;
    ktask->part_id = 0;

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "omp-task");
    # endif /* NDEBUG */

    return ktask;
}

extern "C"
kmp_task_t *
__kmpc_omp_target_task_alloc(
    ident_t * loc_ref,
    kmp_int32 gtid,
    kmp_int32 flags,
    size_t sizeof_kmp_task_t,
    size_t sizeof_shareds,
    kmp_routine_entry_t task_entry,
    kmp_int64 device_id
) {
    kmp_tasking_flags_t & input_flags = reinterpret_cast<kmp_tasking_flags_t &>(flags);

    // target task is untied defined in the specification
    # define TASK_UNTIED    0
    # define TASK_TIED      1
    input_flags.tiedness = TASK_UNTIED;
    // input_flags.target = 1;

    return __kmpc_omp_task_alloc(loc_ref, gtid, flags, sizeof_kmp_task_t, sizeof_shareds, task_entry);
}

extern "C"
kmp_int32
__kmpc_omp_task(
    ident_t * loc_ref,
    kmp_int32 gtid,
    kmp_task_t * ktask
) {
    task_t * task = (task_t *) (((char *) ktask) - task_size);
    assert(task);
    assert(TASK_ARGS(task) == (void *) ktask);

    xkomp_t * xkomp = xkomp_get();
    assert(xkomp);

    xkomp->runtime.task_commit(task);

    return 0;
}

extern "C"

/*!
@ingroup TASKING
@param loc_ref location of the original task directive
@param gtid Global Thread ID of encountering thread
@param new_task task thunk allocated by __kmp_omp_task_alloc() for the ''new
task''
@param ndeps Number of depend items with possible aliasing
@param dep_list List of depend items with possible aliasing
@param ndeps_noalias Number of depend items with no aliasing
@param noalias_dep_list List of depend items with no aliasing

@return Returns either TASK_CURRENT_NOT_QUEUED if the current task was not
suspended and queued, or TASK_CURRENT_QUEUED if it was suspended and queued

Schedule a non-thread-switchable task with dependences for execution
*/
extern "C"
kmp_int32
__kmpc_omp_task_with_deps(
        ident_t * loc_ref,
        kmp_int32 gtid,
        kmp_task_t * new_task,
        kmp_int32 ndeps,
        kmp_depend_info_t * dep_list,
        kmp_int32 ndeps_noalias,
        kmp_depend_info_t * noalias_dep_list
) {
    LOGGER_NOT_IMPLEMENTED();
    return 0;
}

extern "C"
kmp_int32
__kmpc_omp_taskwait(
    ident_t * loc_ref,
    kmp_int32 gtid
) {
    xkomp_t * xkomp = xkomp_get();
    xkomp->runtime.task_wait();
    return 0;
}

static void
body_omp_task(task_t * task)
{
    kmp_task_t * ktask = (kmp_task_t *) TASK_ARGS(task);
    assert(ktask);

    ktask->routine(0, ktask);
}

void
xkomp_task_register_format(xkomp_t * xkomp)
{
    task_format_t format;
    memset(format.f, 0, sizeof(format.f));
    format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_omp_task;
    snprintf(format.label, sizeof(format.label), "omp-task");
    xkomp->task_format = task_format_create(&(xkomp->runtime.formats.list), &format);
}
