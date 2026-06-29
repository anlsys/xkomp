# include <omp.h>

# include <xkomp/xkomp.h>
# include <xkomp/kmp.h>

// task args
typedef struct  task_args_t
{
    task_t * task;
    // followed by the kmp task
}               task_args_t;

/*
 * Number of access slots reserved for a task allocated through the stock-LLVM
 * ABI (__kmpc_omp_task_alloc / __kmpc_omp_target_task_alloc).
 *
 * The patched LLVM passes the dependence/access count at allocation time
 * (__kmpc_omp_task_alloc_with_deps), so XKOMP sizes the task exactly.  Stock
 * LLVM does not: it allocates first and only reveals the dependences later in
 * __kmpc_omp_task_with_deps.  An XKRT task stores its accesses inline *before*
 * the kmp_task_t region, so the kmp_task_t offset depends on the access count,
 * which must therefore be fixed at allocation time.  For the stock ABI we hence
 * reserve a FIXED number of slots and pad the unused ones with inert accesses
 * (keeping the kmp_task_t at a fixed offset); a task needing more dependences
 * than this is rejected.
 */
# define XKOMP_FIXED_ACCESSES   4

// see llvm impl
const size_t
round_up_to_val(size_t size, size_t val)
{
    if (size & (val - 1))
    {
        size &= ~(val - 1);
        if (size <= SIZE_MAX - val)
            size += val;
    }
    return size;
}

static inline kmp_task_t *
ktask_from_task(task_t * task)
{
    task_args_t * args = (task_args_t *) TASK_ARGS(task);
    assert(args);

    kmp_task_t * ktask = (kmp_task_t *) (args + 1);
    assert(ktask);

    return ktask;
}

static inline task_t *
task_from_ktask(kmp_task_t * ktask)
{
    assert(ktask);

    task_args_t * args = (task_args_t *) (((char *) ktask) - sizeof(task_args_t));
    assert(args);
    assert((void *)(args + 1) == (void *) ktask);
    assert(args->task);
    // assert(TASK_ARGS(args->task) == (void *) args);

    return args->task;
}

/*
 * Pre-fill access slots [from, to) with inert "virtual" (ACCESS_MODE_V) accesses.
 * They carry no data movement and have no successors, so every runtime loop that
 * iterates over the task's reserved slots (data fetch, dependence release,
 * taskgraph walk) skips/ignores them.  This keeps acs->ac (and hence the
 * kmp_task_t offset used by ktask_from_task) fixed, while real dependences (if
 * any) overwrite the leading slots in __kmpc_omp_task_with_deps_v2.
 */
static inline void
task_pad_accesses(task_t * task, access_t * accesses, kmp_int32 from, kmp_int32 to)
{
    for (kmp_int32 i = from ; i < to ; ++i)
        new (accesses + i) access_t(task, ACCESS_MODE_V, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
}

// Launch the routine associated with an openmp task
static inline void
body_omp_task_run(
    task_t * task,
    int32_t gtid
) {
    kmp_task_t * ktask = ktask_from_task(task);
    assert(ktask);
    assert(ktask->routine);
    ktask->routine(gtid, ktask);
}

// Called on replay of a task
static inline void
body_omp_task_replay(void * args[XKRT_CALLBACK_ARGS_MAX])
{
    runtime_t * runtime       = (runtime_t *) args[0];
    task_t    * original_task = (task_t *)    args[1];
    command_t * cmd           = (command_t *) args[2];
    assert(runtime);
    assert(original_task);
    assert(cmd);

    // TODO: instead, dupplicate that task and its data environments
    LOGGER_WARN("TODO: dupplicate also data environment - and save 'original task' in it");
    runtime->task_spawn([=] (runtime_t * runtime, device_t *, task_t * replay_task) {
        # if 0
        thread_t * thread = thread_t::get_tls();
        assert(thread);
        assert(thread->team);
        const int gtid = thread->gtid;
        # else
        constexpr int gtid = 0;
        # endif
        body_omp_task_run(original_task, gtid);
        cmd->completion_callback_raise();   // complete command after task replayed
    });
}

// Called from a non-device thread
static inline void
body_omp_task(
    runtime_t * runtime,
    device_t * device,
    task_t * task
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);
    assert(thread->team);

    body_omp_task_run(task, thread->gtid);

    // if recording, insert a new command for replay
    if (task->flags & TASK_FLAG_RECORD)
    {
        task_rec_info_t * rec = TASK_REC_INFO(task);
        assert(rec);

        /* insert a host routine that submit a new task when the command is ready */
        task_command_record_t * cmdrec = rec->commands.put();
        constexpr cgir::command_type_t ctype = cgir::COMMAND_TYPE_PROG;
        constexpr command_flag_t flags = COMMAND_FLAG_SYNCHRONOUS | COMMAND_FLAG_SERIALIZED;
        cmdrec->state = task->state.value;
        new (&cmdrec->command) command_t(ctype, flags);
        cmdrec->command.prog.launcher.fixed.fn      = body_omp_task_replay;
        cmdrec->command.prog.launcher.fixed.args[0] = runtime;
        cmdrec->command.prog.launcher.fixed.args[1] = task;
        cmdrec->command.prog.launcher.fixed.args[2] = &cmdrec->command;
        static_assert(CGIR_CALLBACK_ARGS_MAX >= 3);
    }
}

// called from a device thread
static inline void
body_omp_task_target(
    runtime_t * runtime,
    device_t * device,
    task_t * task
) {
    thread_t * thread = thread_t::get_tls();
    int32_t gtid = thread->gtid;
    body_omp_task_run(task, gtid);
}

// Get (or lazily create) the task format for a source location (ident_t *
// loc_ref), copying the per-target functions from the shared 'omp-task'
// template and attaching the construct's LLVM-IR.
static inline task_format_id_t
get_or_create_loc_format(
    xkomp_t * xkomp,
    ident_t * loc_ref,
    void * ir,
    size_t ir_size
) {
    // no source location: fall back to the shared template format
    if (loc_ref == NULL)
        return xkomp->formats.kmp.host;

    void * key = (void *) loc_ref;

    SPINLOCK_LOCK(xkomp->formats.kmp.per_loc_lock);

    task_format_id_t fmtid;
    auto it = xkomp->formats.kmp.per_loc.find(key);
    if (it != xkomp->formats.kmp.per_loc.end())
    {
        fmtid = it->second;
    }
    else
    {
        task_format_t format;
        memset(&format, 0, sizeof(task_format_t));

        task_format_t * tmpl = xkomp->runtime.task_format_get(xkomp->formats.kmp.host);
        memcpy(format.f, tmpl->f, sizeof(format.f));
        format.suggest = tmpl->suggest;

        // label from the source location string ("file;func;line;line")
        if (loc_ref->psource)
            snprintf(format.label, sizeof(format.label), "%s", loc_ref->psource);
        else
            snprintf(format.label, sizeof(format.label), "omp-task");

        // the IR is a compile-time global, so `_owned` is false
        if (ir != NULL && ir_size > 0)
        {
            cgir_command_prog_source_t * src = &format.source[XKRT_TASK_FORMAT_TARGET_HOST];
            src->type                  = CGIR_COMMAND_PROG_SOURCE_TYPE_LLVMIR;
            src->content.llvmir.raw    = ir;
            src->content.llvmir.size   = ir_size;
            src->content.llvmir._owned = false;
        }

        fmtid = xkomp->runtime.task_format_create(&format);
        xkomp->formats.kmp.per_loc[key] = fmtid;
    }

    SPINLOCK_UNLOCK(xkomp->formats.kmp.per_loc_lock);
    return fmtid;
}

inline static kmp_task *
task_alloc(
    ident_t * loc_ref,
    kmp_int32 gtid,
    kmp_tasking_flags_t flags,
    size_t sizeof_kmp_task_t,
    size_t sizeof_shareds,
    kmp_routine_entry_t task_entry,
    kmp_int32 ndeps,
    kmp_int32 nacs,
    kmp_int32 device_id,
    void * ir,
    size_t ir_size
) {
    if (device_id == -1)
        device_id = omp_get_default_device();

    // there is a '1' offset between omp device id and xkaapi device id
    static_assert(XKRT_HOST_DEVICE_UNIQUE_ID == 0);
    device_unique_id_t device_unique_id = omp_device_id_to_xkomp(device_id);

    const kmp_int32 naccesses = ndeps + nacs;
    assert(naccesses <= XKRT_TASK_MAX_ACCESSES);

    thread_t * thread = thread_t::get_tls();
    assert(thread);

    xkomp_t * xkomp = xkomp_get();
    assert(xkomp);

    // compute flags
    task_flag_bitfield_t xkflags  = TASK_FLAG_ZERO;

    // if tasks has accesses, set flag
    if (naccesses)
        xkflags |= TASK_FLAG_ACCESSES;

    // if task is not running on the host, set device/detachable flags, since libomptarget may submit commands
    if (device_unique_id != XKRT_HOST_DEVICE_UNIQUE_ID)
    {
        xkflags |= TASK_FLAG_DEVICE;        // execute on a device thread
        xkflags |= TASK_FLAG_DETACHABLE;    // a device task may submit commands
    }

    // if recording flag is set on parent, record that task.
    // TODO: if the `replayable(false)` is set, do not record

    // resolve the per-source-location task format (carries the LLVM-IR)
    const task_format_id_t fmtid = get_or_create_loc_format(xkomp, loc_ref, ir, ir_size);

    // see llvm impl
    const size_t args_size = sizeof(task_args_t) + round_up_to_val(sizeof_kmp_task_t + sizeof_shareds, sizeof(void *));
    task_t * task = xkomp->runtime.task_new(fmtid, xkflags, NULL, args_size, naccesses);
    assert(task);

    // init acs infos
    if (naccesses)
    {
        task_acs_info_t * acs = TASK_ACS_INFO(task);

        // if spawning a device task, set spawning_thread so that
        // successor tasks are pushed to it
        thread_t * spawning_thread = (device_unique_id == XKRT_HOST_DEVICE_UNIQUE_ID) ? thread : NULL;

        new (acs) task_acs_info_t(spawning_thread, naccesses);
    }

    // init dev/det infos
    if (device_unique_id != XKRT_HOST_DEVICE_UNIQUE_ID)
    {
        assert(xkflags & TASK_FLAG_DEVICE);
        task_dev_info_t * dev = TASK_DEV_INFO(task);
        new (dev) task_dev_info_t(device_unique_id, XKRT_UNSPECIFIED_TASK_ACCESS);

        assert(xkflags & TASK_FLAG_DETACHABLE);
        task_det_info_t * det = TASK_DET_INFO(task);
        new (det) task_det_info_t();
    }

    // init args
    task_args_t * args = (task_args_t *) TASK_ARGS(task);
    assert(args);
    args->task = task;

    // init kmp task info
    kmp_task_t * ktask = (kmp_task_t *) (args + 1);
    assert(ktask);

    ktask->shareds = (void *) (sizeof_shareds <= 0 ? 0 : ((uintptr_t) ktask) + sizeof_kmp_task_t);
    ktask->routine = task_entry;
    ktask->part_id = 0;

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "omp-task");
    # endif /* NDEBUG */

    return ktask;
}

extern "C"
kmp_task_t *
__kmpc_omp_task_alloc_with_deps(
    ident_t * loc_ref,
    kmp_int32 gtid,
    kmp_tasking_flags_t flags,
    size_t sizeof_kmp_task_t,
    size_t sizeof_shareds,
    kmp_routine_entry_t task_entry,
    kmp_int32 ndeps,
    kmp_int32 nacs,
    void * ir,
    size_t ir_size
) {
    const kmp_int32 device_id = omp_get_initial_device();
    return task_alloc(loc_ref, gtid, flags, sizeof_kmp_task_t, sizeof_shareds, task_entry, ndeps, nacs, device_id, ir, ir_size);
}

extern "C"
kmp_task_t *
__kmpc_omp_target_task_alloc_with_deps(
    ident_t * loc_ref,
    kmp_int32 gtid,
    kmp_tasking_flags_t flags,
    size_t sizeof_kmp_task_t,
    size_t sizeof_shareds,
    kmp_routine_entry_t task_entry,
    kmp_int64 device_id,
    kmp_int32 ndeps,
    kmp_int32 nacs,
    void * ir,
    size_t ir_size
) {
    // target task is untied defined in the specification
    # define TASK_UNTIED    0
    # define TASK_TIED      1
    flags.tiedness = TASK_UNTIED;

    return task_alloc(loc_ref, gtid, flags, sizeof_kmp_task_t, sizeof_shareds, task_entry, ndeps, nacs, device_id, ir, ir_size);
}

// Stock-LLVM ABI entry point (no dependence count at alloc time): reserve a
// FIXED number of access slots and pad them with inert accesses, keeping the
// kmp_task_t at a fixed offset.  Real dependences (if any, up to
// XKOMP_FIXED_ACCESSES) overwrite the leading slots in __kmpc_omp_task_with_deps.
extern "C"
kmp_task_t *
__kmpc_omp_target_task_alloc(
    ident_t * loc_ref,
    kmp_int32 gtid,
    kmp_tasking_flags_t flags,
    size_t sizeof_kmp_task_t,
    size_t sizeof_shareds,
    kmp_routine_entry_t task_entry,
    kmp_int64 device_id
) {
    LOGGER_WARN("Stock LLVM/clang ABI: at most %d dependences/accesses per task "
                "are supported; use the patched LLVM/clang for XKOMP for unlimited "
                "dependences and best performance.", (int) XKOMP_FIXED_ACCESSES);
    flags.tiedness = TASK_UNTIED;   // target tasks are untied per the spec
    constexpr kmp_int32 ndeps = XKOMP_FIXED_ACCESSES;
    constexpr kmp_int32 nacs  = 0;
    constexpr void * ir = NULL;
    constexpr size_t ir_size = 0;
    kmp_task_t * ktask = task_alloc(loc_ref, gtid, flags, sizeof_kmp_task_t, sizeof_shareds, task_entry, ndeps, nacs, device_id, ir, ir_size);
    task_t * task = task_from_ktask(ktask);
    task_pad_accesses(task, TASK_ACCESSES(task), 0, XKOMP_FIXED_ACCESSES);
    return ktask;
}

// Stock-LLVM ABI entry point: see __kmpc_omp_target_task_alloc above.
extern "C"
kmp_task_t *
__kmpc_omp_task_alloc(
    ident_t * loc_ref,
    kmp_int32 gtid,
    kmp_tasking_flags_t flags,
    size_t sizeof_kmp_task_t,
    size_t sizeof_shareds,
    kmp_routine_entry_t task_entry
) {
    LOGGER_WARN("Stock LLVM/clang ABI: at most %d dependences/accesses per task "
                "are supported; use the patched LLVM/clang for XKOMP for unlimited "
                "dependences and best performance.", (int) XKOMP_FIXED_ACCESSES);
    constexpr kmp_int32 ndeps = XKOMP_FIXED_ACCESSES;
    constexpr kmp_int32 nacs  = 0;
    constexpr void * ir = NULL;
    constexpr size_t ir_size = 0;
    const kmp_int32 device_id = omp_get_initial_device();
    kmp_task_t * ktask = task_alloc(loc_ref, gtid, flags, sizeof_kmp_task_t, sizeof_shareds, task_entry, ndeps, nacs, device_id, ir, ir_size);
    task_t * task = task_from_ktask(ktask);
    task_pad_accesses(task, TASK_ACCESSES(task), 0, XKOMP_FIXED_ACCESSES);
    return ktask;
}

extern "C"
kmp_int32
__kmpc_omp_task(
    ident_t * loc_ref,
    kmp_int32 gtid,
    kmp_task_t * ktask
) {
    task_t * task = task_from_ktask(ktask);

    xkomp_t * xkomp = xkomp_get();
    assert(xkomp);

    // TODO: im not sure why we need all that mess, see MPC and LLVM impl to
    // figure out what's going on with untied tasks
    /* LLVM calls both
     *  '__kmpc_omp_task' and '__kmpc_omp_task_with_deps'
     * if a task is untied and has dependences.
     * In such case, the task was already commit in the previous call to
     * '__kmpc_omp_task_with_deps'
     */
    if (task->state.value == TASK_STATE_EXECUTING)
    {
        body_omp_task(NULL, NULL, task);
        return 1;
    }

    // stock-LLVM no-dep task: its reserved access slots were already padded with
    // inert accesses at allocation time, so it can be committed as-is.
    xkomp->runtime.task_commit(task);

    return 0;
}

extern "C"
void
__kmp_omp_task_acs_to_xkomp_accesses(
    task_t * task,
    access_t * accesses,
    kmp_access_info_t * acs_list,
    kmp_int32 nacs
) {
    for (kmp_int32 i = 0; i < nacs ; ++i)
    {
        const void             * ptr = (const void *) acs_list[i].base_addr;
        const size_t               n = (size_t) acs_list[i].len;
        constexpr size_t sizeof_type = 1;
        if (ptr)
        {
            access_mode_t           mode        = (access_mode_t) 0;
            access_concurrency_t    concurrency = ACCESS_CONCURRENCY_SEQUENTIAL;
            access_scope_t          scope       = ACCESS_SCOPE_NONUNIFIED;

            if (acs_list[i].flags.storage)
            {
                acs_list[i].flags.write = 1;
                acs_list[i].flags.noncoherent = 1;
            }

            if (acs_list[i].flags.read)
                mode = (access_mode_t) (mode | ACCESS_MODE_R);

            if (acs_list[i].flags.write || acs_list[i].flags.concurrentwrite)
            {
                mode = (access_mode_t) (mode | ACCESS_MODE_W);
                if (acs_list[i].flags.concurrentwrite)
                    concurrency = ACCESS_CONCURRENCY_CONCURRENT;
            }

            if (acs_list[i].flags.noncoherent)
                mode = (access_mode_t) (mode | ACCESS_MODE_V);

            if (acs_list[i].flags.nostorage)
                LOGGER_FATAL("XKRT does not support the no-storage modifier yet");

            new (accesses + i) access_t(task, ptr, n, sizeof_type, mode, concurrency, scope);
        }
    }
}

extern "C"
void
__kmp_omp_task_deps_to_xkomp_accesses(
    task_t * task,
    access_t * accesses,
    kmp_depend_info_t * dep_list,
    kmp_int32 ndeps
) {
    for (kmp_int32 i = 0; i < ndeps ; ++i)
    {
        const void * ptr = (const void *) dep_list[i].base_addr;
        if (ptr)
        {
            access_mode_t           mode        = ACCESS_MODE_V;
            access_concurrency_t    concurrency = ACCESS_CONCURRENCY_SEQUENTIAL;
            access_scope_t          scope       = ACCESS_SCOPE_NONUNIFIED;

            if (dep_list[i].flags.in)
            {
                mode = (access_mode_t) (mode | ACCESS_MODE_R);
                concurrency = ACCESS_CONCURRENCY_SEQUENTIAL;
            }
            if (dep_list[i].flags.out)
            {
                mode = (access_mode_t) (mode | ACCESS_MODE_W);
                concurrency = ACCESS_CONCURRENCY_SEQUENTIAL;
            }
            if (dep_list[i].flags.mtx)
            {
                mode = (access_mode_t) (mode | ACCESS_MODE_W);
                concurrency = ACCESS_CONCURRENCY_COMMUTATIVE;
            }
            if (dep_list[i].flags.set)
            {
                mode = (access_mode_t) (mode | ACCESS_MODE_W);
                concurrency = ACCESS_CONCURRENCY_CONCURRENT;
            }
            if (dep_list[i].flags.all)
            {
                LOGGER_FATAL("Not implemented");
            }

            // new (accesses + access_idx++) access_t(task, ptr, 1, 1, mode, concurrency, scope);
            new (accesses + i) access_t(task, ptr, mode, concurrency, scope);
        }
    }
}

/*!
@ingroup TASKING
@param loc_ref location of the original task directive
@param gtid Global Thread ID of encountering thread
@param ktask task thunk allocated by __kmp_omp_task_alloc() for the ''new
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
__kmpc_omp_task_with_deps_v2(
    ident_t * loc_ref,
    kmp_int32 gtid,
    kmp_task_t * ktask,
    kmp_int32 ndeps,
    kmp_depend_info_t * dep_list,
    kmp_int32 ndeps_noalias,
    kmp_depend_info_t * noalias_dep_list,
    kmp_int32 nacs,
    kmp_access_info_t * acs_list
) {
    xkomp_t * xkomp = xkomp_get();
    assert(xkomp);

    // total accesses = access-clause (XKOMP ext) + aliased deps + non-aliased deps.
    // Stock LLVM splits dependences into an aliasing and a non-aliasing list;
    // both must be honoured (XKOMP previously ignored the non-aliasing one).
    const kmp_int32 naccesses = nacs + ndeps + ndeps_noalias;

    task_t * task = task_from_ktask(ktask);

    if (naccesses)
    {
        task_acs_info_t * acs = TASK_ACS_INFO(task);

        // The task was sized at allocation time: exactly (ndeps+nacs) for the
        // patched ABI, or XKOMP_FIXED_ACCESSES reserved (and padded) slots for the
        // stock ABI.  More accesses than were reserved cannot fit in the inline
        // array, so reject loudly.
        if (naccesses > acs->ac)
            LOGGER_FATAL("OpenMP task needs %d dependences/accesses but the stock "
                         "LLVM/clang ABI only supports %d per task; use the patched "
                         "LLVM/clang for XKOMP.",
                         (int) naccesses, (int) acs->ac);

        access_t * accesses = TASK_ACCESSES(task);

        /* access clause (XKOMP extension) */
        __kmp_omp_task_acs_to_xkomp_accesses(task, accesses, acs_list, nacs);

        /* depend clause: aliasing deps, then non-aliasing deps */
        __kmp_omp_task_deps_to_xkomp_accesses(task, accesses + nacs, dep_list, ndeps);
        __kmp_omp_task_deps_to_xkomp_accesses(task, accesses + nacs + ndeps, noalias_dep_list, ndeps_noalias);

        // resolve only the real accesses; the trailing reserved slots keep the
        // inert padding installed at allocation time.
        xkomp->runtime.task_accesses_resolve(accesses, naccesses);
    }

    xkomp->runtime.task_commit(task);

    # define TASK_CURRENT_NOT_QUEUED    0
    # define TASK_CURRENT_QUEUED        1
    return TASK_CURRENT_QUEUED;
}



extern "C"
kmp_int32
__kmpc_omp_task_with_deps(
    ident_t * loc_ref,
    kmp_int32 gtid,
    kmp_task_t * ktask,
    kmp_int32 ndeps,
    kmp_depend_info_t * dep_list,
    kmp_int32 ndeps_noalias,
    kmp_depend_info_t * noalias_dep_list
) {
    return __kmpc_omp_task_with_deps_v2(loc_ref, gtid, ktask, ndeps, dep_list, ndeps_noalias, noalias_dep_list, 0, NULL);
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

extern "C"
void
__kmpc_omp_wait_deps(ident_t *loc_ref, kmp_int32 gtid, kmp_int32 ndeps,
        kmp_depend_info_t *dep_list, kmp_int32 ndeps_noalias,
        kmp_depend_info_t *noalias_dep_list) {
}

void
xkomp_task_register_formats_kmp_task(xkomp_t * xkomp)
{
    task_format_t format;
    memset(&format, 0, sizeof(task_format_t));
    for (int i = 0 ; i < XKRT_TASK_FORMAT_TARGET_MAX ; ++i)
        format.f[i] = (task_format_func_t) body_omp_task_target;
    format.f[XKRT_TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_omp_task;
    snprintf(format.label, sizeof(format.label), "omp-task");
    xkomp->formats.kmp.host = xkomp->runtime.task_format_create(&format);
}
