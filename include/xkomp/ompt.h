/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Thierry Gautier, thierry.gautier@inrialpes.fr
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

/**
 * XKOMP OMPT bridge
 * =================
 *
 * Implements the OpenMP Tools interface (OMPT) on top of the XKRT tooling
 * interface (XKRT-T). It is a hybrid, mirroring libgomp's design:
 *   - runtime-internal events (thread/task lifecycle, dependences, barriers,
 *     taskwait, taskgroup) come from XKRT-T callbacks the bridge registers;
 *   - OpenMP-construct events XKRT does not model (parallel region begin/end
 *     and implicit tasks) are emitted directly from XKOMP's own ABI code via
 *     the helpers declared below.
 *
 * The user's OMPT tool is discovered the standard way (weak `ompt_start_tool`
 * then `OMP_TOOL_LIBRARIES`) from inside the bridge's XKRT-T `initialize`.
 *
 * Do NOT include this header directly: <xkomp/xkomp.h> pulls it in when OMPT is
 * enabled (and defines XKOMP_OMPT_EMIT() as a no-op otherwise). It therefore
 * relies on runtime_t/team_t/thread_t and XKRT_NAMESPACE_USE from xkomp.h.
 *
 * Call sites emit an event with the XKOMP_OMPT_EMIT() macro, e.g.
 *     XKOMP_OMPT_EMIT(implicit_task_begin, thread);
 * which forwards to the matching xkomp_ompt_<name> helper below, or expands to a
 * no-op (arguments unevaluated) when OMPT is compiled out.
 */

#ifndef __XKOMP_OMPT_H__
# define __XKOMP_OMPT_H__

/**
 * Register the OMPT bridge with `runtime` as an in-process XKRT-T tool. Must be
 * called before `runtime.init()` (which triggers OMPT tool discovery).
 */
void xkomp_ompt_connect(runtime_t * runtime);

/* ---- construct-level OMPT events emitted directly by XKOMP ---------------- */

/** `parallel` region begin, on the encountering thread. `team->tool_data` backs
 *  the region's `ompt_data_t` (parallel_data). */
void xkomp_ompt_parallel_begin(team_t * team, thread_t * encountering, unsigned int nthreads, const void * codeptr);

/** `parallel` region end, on the encountering thread. */
void xkomp_ompt_parallel_end(team_t * team, thread_t * encountering, const void * codeptr);

/** implicit task begin, on each team member (run in the fork wrapper). */
void xkomp_ompt_implicit_task_begin(thread_t * thread);

/** implicit task end, on each team member (run in the fork wrapper). */
void xkomp_ompt_implicit_task_end(thread_t * thread);

/**
 * Emit an OMPT event by forwarding to its xkomp_ompt_<name> helper. The no-op
 * variant (OMPT disabled) is defined in <xkomp/xkomp.h>.
 *   XKOMP_OMPT_EMIT(parallel_begin, &team, tls, n, NULL);
 */
# define XKOMP_OMPT_EMIT(name, ...) xkomp_ompt_##name(__VA_ARGS__)

#endif /* __XKOMP_OMPT_H__ */
