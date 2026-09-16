/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Romain PEREIRA, romain.pereira@inria.fr + rpereira@anl.gov
**
** This software is a computer program whose purpose is to execute
** blas subroutines on multi-GPUs system.
**
** This software is governed by the CeCILL-C license under French law and
** abiding by the rules of distribution of free software.  You can  use,
** modify and/ or redistribute the software under the terms of the CeCILL-C
** license as circulated by CEA, CNRS and INRIA at the following URL
** "http://www.cecill.info".

** As a counterpart to the access to the source code and  rights to copy,
** modify and redistribute granted by the license, users are provided only
** with a limited warranty  and the software's author,  the holder of the
** economic rights,  and the successive licensors  have only  limited
** liability.

** In this respect, the user's attention is drawn to the risks associated
** with loading,  using,  modifying and/or developing or reproducing the
** software by the user in light of its specific status of free software,
** that may mean  that it is complicated to manipulate,  and  that  also
** therefore means  that it is reserved for developers  and  experienced
** professionals having in-depth computer knowledge. Users are therefore
** encouraged to load and test the software's suitability as regards their
** requirements in conditions enabling the security of their systems and/or
** data to be ensured and,  more generally, to use and operate it in the
** same conditions as regards security.

** The fact that you are presently reading this means that you have had
** knowledge of the CeCILL-C license and that you accept its terms.
**/

/*
 * A minimal, standard OMPT tool used by the `ompt/driver` test. It is loaded by
 * XKOMP through OMP_TOOL_LIBRARIES, registers the host callbacks the XKOMP OMPT
 * bridge emits, counts them, and prints "XKOMP-OMPT:OK" from its finalize when
 * every expected callback has fired.
 */

# include <omp-tools.h>

# include <atomic>
# include <cstdint>
# include <cstdio>

static std::atomic<uint64_t> COUNT[ompt_callback_error + 1];
static ompt_set_callback_t   set_callback = nullptr;

static inline void bump(int e) { COUNT[e].fetch_add(1, std::memory_order_relaxed); }

static void on_thread_begin(ompt_thread_t type, ompt_data_t * d)
{ (void) type; (void) d; bump(ompt_callback_thread_begin); }

static void on_thread_end(ompt_data_t * d)
{ (void) d; bump(ompt_callback_thread_end); }

static void on_parallel_begin(ompt_data_t * enc, const ompt_frame_t * f, ompt_data_t * par,
                              unsigned int req, int flags, const void * ra)
{ (void) enc; (void) f; (void) par; (void) req; (void) flags; (void) ra; bump(ompt_callback_parallel_begin); }

static void on_parallel_end(ompt_data_t * par, ompt_data_t * enc, int flags, const void * ra)
{ (void) par; (void) enc; (void) flags; (void) ra; bump(ompt_callback_parallel_end); }

static void on_implicit_task(ompt_scope_endpoint_t ep, ompt_data_t * par, ompt_data_t * task,
                             unsigned int ap, unsigned int idx, int flags)
{ (void) ep; (void) par; (void) task; (void) ap; (void) idx; (void) flags; bump(ompt_callback_implicit_task); }

static void on_task_create(ompt_data_t * enc, const ompt_frame_t * f, ompt_data_t * nt,
                           int flags, int has_deps, const void * ra)
{ (void) enc; (void) f; (void) nt; (void) flags; (void) has_deps; (void) ra; bump(ompt_callback_task_create); }

static void on_task_schedule(ompt_data_t * prior, ompt_task_status_t st, ompt_data_t * next)
{ (void) prior; (void) st; (void) next; bump(ompt_callback_task_schedule); }

static void on_dependences(ompt_data_t * task, const ompt_dependence_t * deps, int ndeps)
{ (void) task; (void) deps; (void) ndeps; bump(ompt_callback_dependences); }

static void on_sync_region(ompt_sync_region_t kind, ompt_scope_endpoint_t ep, ompt_data_t * par,
                           ompt_data_t * task, const void * ra)
{ (void) kind; (void) ep; (void) par; (void) task; (void) ra; bump(ompt_callback_sync_region); }

static int
tool_initialize(ompt_function_lookup_t lookup, int initial_device, ompt_data_t * tool_data)
{
    (void) initial_device; (void) tool_data;

    set_callback = (ompt_set_callback_t) lookup("ompt_set_callback");
    if (set_callback == nullptr)
    {
        fprintf(stderr, "[ompt-tool] could not resolve ompt_set_callback\n");
        return 0;
    }

    # define REG(name, fn) set_callback((ompt_callbacks_t) name, (ompt_callback_t) fn)
    REG(ompt_callback_thread_begin,    on_thread_begin);
    REG(ompt_callback_thread_end,      on_thread_end);
    REG(ompt_callback_parallel_begin,  on_parallel_begin);
    REG(ompt_callback_parallel_end,    on_parallel_end);
    REG(ompt_callback_implicit_task,   on_implicit_task);
    REG(ompt_callback_task_create,     on_task_create);
    REG(ompt_callback_task_schedule,   on_task_schedule);
    REG(ompt_callback_dependences,     on_dependences);
    REG(ompt_callback_sync_region,     on_sync_region);
    REG(ompt_callback_sync_region_wait, on_sync_region);
    # undef REG

    fprintf(stdout, "[ompt-tool] initialized\n");
    fflush(stdout);
    return 1;
}

static void
tool_finalize(ompt_data_t * tool_data)
{
    (void) tool_data;

    static const struct { int ev; const char * name; } NAMES[] = {
        { ompt_callback_thread_begin,   "thread_begin"   },
        { ompt_callback_thread_end,     "thread_end"     },
        { ompt_callback_parallel_begin, "parallel_begin" },
        { ompt_callback_parallel_end,   "parallel_end"   },
        { ompt_callback_implicit_task,  "implicit_task"  },
        { ompt_callback_task_create,    "task_create"    },
        { ompt_callback_task_schedule,  "task_schedule"  },
        { ompt_callback_dependences,    "dependences"    },
        { ompt_callback_sync_region,    "sync_region"    },
    };

    bool all = true;
    for (auto & e : NAMES)
    {
        uint64_t c = COUNT[e.ev].load(std::memory_order_relaxed);
        fprintf(stdout, "[ompt-tool] %-16s = %llu\n", e.name, (unsigned long long) c);
        if (c == 0)
            all = false;
    }
    fprintf(stdout, all ? "XKOMP-OMPT:OK\n" : "XKOMP-OMPT:MISSING\n");
    fflush(stdout);
}

extern "C" ompt_start_tool_result_t *
ompt_start_tool(unsigned int omp_version, const char * runtime_version)
{
    (void) omp_version; (void) runtime_version;
    static ompt_start_tool_result_t result = { &tool_initialize, &tool_finalize, { 0 } };
    return &result;
}
