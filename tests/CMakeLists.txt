cmake_minimum_required(VERSION 3.20)
include(CTest)

project(tests CXX)

#
# XKOMP test-suite
# ----------------
# Two top-level families, each then organised by construct:
#
#   directive/   standard C/C++ OpenMP, written with `#pragma omp ...` only
#                (parallel, worksharing/{for,sections,single}, master, masked,
#                 barrier, atomic, critical, tasking, teams, composite, target,
#                 access)
#   xkomp/       tests of the xkomp++.h C++ interfaces (e.g.
#                pragma_omp_taskgraph(gid, flags, [&]{ ... }))
#
# The source-tree hierarchy is mirrored in the build tree: a test at
#   tests/<family>/<construct>/<name>.cc
# is built to
#   <build>/tests/<family>/<construct>/<name>
# (so two tests may share a short name without colliding).
#
# Each test documents its status at the top of the file:
#   // xkomp: supported          -> expected to PASS
#   // xkomp: XFAIL <reason>      -> construct not supported yet; registered with
#                                    xkomp_add_xfail_test() so it shows up as
#                                    FAILED in `make test` (gap tracking) until
#                                    the construct is implemented.
#
# All tests are compiled with -fopenmp (otherwise clang silently drops every
# `#pragma omp`) and linked against the in-tree xkomp library so the
# __kmpc_*/omp_* symbols resolve to it (libomp.so/libiomp5.so/libgomp.so are
# symlinks onto libxkomp.so created by the top-level build).
#
# Correctness is checked with assert() (see common.h), so the suite must be
# configured in Debug mode.
#

# --- enforce Debug so that asserts are live ----------------------------------
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR
        "The XKOMP test-suite checks correctness with assert(), so it must be "
        "configured in Debug mode.\nRe-run cmake with -DCMAKE_BUILD_TYPE=Debug.")
endif()

find_package(HWLOC REQUIRED)
find_package(XKRT  REQUIRED)

option(XKOMP_TESTS_TARGET
       "Build tests that use 'omp target' (require the offload runtime)" ON)

# The 'access' clause is an XKOMP extension that only the patched LLVM/clang can
# parse (https://github.com/anlsys/llvm-project, iwomp26-access-clause).  Turn
# this OFF when building the suite with a stock compiler.
option(XKOMP_TESTS_ACCESS
       "Build tests for the XKOMP 'access' clause (require the patched LLVM/clang)" ON)

set(XKOMP_TEST_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}        # tests/common.h
    ${CMAKE_SOURCE_DIR}/include        # <xkomp/*.h>
    ${CMAKE_BINARY_DIR}/include)       # generated <xkomp/support.h>

# Common settings shared by every test target.
function(_xkomp_test_setup tgt)
    target_compile_features(${tgt} PRIVATE cxx_std_20)
    # -fopenmp: lower the pragmas; -UNDEBUG: keep assert() live regardless.
    target_compile_options(${tgt} PRIVATE -fopenmp -g -UNDEBUG)
    target_link_options(${tgt}    PRIVATE -fopenmp)
    target_include_directories(${tgt} PRIVATE ${XKOMP_TEST_INCLUDES})
    # Put the top-level build dir first on the link path so the `-lomp` that
    # -fopenmp appends resolves to the libomp.so -> libxkomp.so symlink (whose
    # SONAME is libxkomp.so) rather than to clang's own libomp.
    target_link_directories(${tgt} BEFORE PRIVATE ${CMAKE_BINARY_DIR})
    if(TARGET compat_links)
        add_dependencies(${tgt} compat_links)
    endif()
    target_link_libraries(${tgt} PRIVATE ${XKOMP} ${HWLOC_LIBRARIES} m)
    # The xkomp/ (xkomp++.h) tests include <xkrt/*.h>; link the imported target so its
    # include directories propagate (xkomp links XKRT privately, so it does not
    # carry them transitively).  Fall back to the plain variables otherwise.
    if(TARGET XKRT::xkrt)
        target_link_libraries(${tgt} PRIVATE XKRT::xkrt)
    else()
        target_link_libraries(${tgt} PRIVATE ${XKRT_LIBRARIES})
        target_include_directories(${tgt} PRIVATE ${XKRT_INCLUDE_DIRS})
    endif()
endfunction()

# Derive, from a source path "<dir>/<base>.cc", a unique CMake target name, the
# mirrored output directory and the (hierarchical) CTest name.
macro(_xkomp_test_paths src)
    get_filename_component(_xk_dir  "${src}" DIRECTORY)   # e.g. directive/parallel
    get_filename_component(_xk_base "${src}" NAME_WE)     # e.g. omp_parallel
    set(_xk_test "${_xk_dir}/${_xk_base}")                # CTest name
    string(REGEX REPLACE "[^A-Za-z0-9]" "_" _xk_tgt "${_xk_test}")  # CMake target
endmacro()

# place the built executable under <build>/tests/<dir>/ with name <base>
function(_xkomp_test_output tgt dir base)
    set_target_properties(${tgt} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${dir}"
        OUTPUT_NAME "${base}")
endfunction()

# Expected-to-PASS test.  usage: xkomp_add_test(<source> <label>)
function(xkomp_add_test src label)
    _xkomp_test_paths("${src}")
    add_executable(${_xk_tgt} ${src})
    _xkomp_test_setup(${_xk_tgt})
    _xkomp_test_output(${_xk_tgt} "${_xk_dir}" "${_xk_base}")
    add_test(NAME "${_xk_test}" COMMAND ${_xk_tgt})
    set_tests_properties("${_xk_test}" PROPERTIES LABELS "${label}")
endfunction()

# Test for a construct XKOMP does not support yet.  Built on demand (so a link
# failure does not break `make`) and driven through run_xfail.cmake, which runs
# the test as a normal FAILURE when the construct cannot be compiled/linked or
# aborts at run time.  It therefore shows up as "Failed" in `make test` -- so the
# gap is tracked -- and turns "Passed" once the construct is implemented.
function(xkomp_add_xfail_test src label)
    _xkomp_test_paths("${src}")
    add_executable(${_xk_tgt} EXCLUDE_FROM_ALL ${src})
    _xkomp_test_setup(${_xk_tgt})
    _xkomp_test_output(${_xk_tgt} "${_xk_dir}" "${_xk_base}")
    add_test(NAME "${_xk_test}"
        COMMAND ${CMAKE_COMMAND}
            -DTARGET=${_xk_tgt}
            -DBIN_DIR=${CMAKE_BINARY_DIR}
            -DEXE=$<TARGET_FILE:${_xk_tgt}>
            -P ${CMAKE_SOURCE_DIR}/cmake/run_xfail.cmake)
    set_tests_properties("${_xk_test}" PROPERTIES LABELS "unsupported;${label}")
endfunction()

##############################################################################
# directive/ : standard C/C++ OpenMP (#pragma omp ...)                         #
##############################################################################

# --- parallel ---------------------------------------------------------------
xkomp_add_test(directive/parallel/omp_parallel.cc               parallel)
xkomp_add_test(directive/parallel/omp_parallel_num_threads.cc   parallel)
xkomp_add_test(directive/parallel/omp_parallel_firstprivate.cc  parallel)
xkomp_add_test(directive/parallel/omp_parallel_nested.cc        parallel)
xkomp_add_xfail_test(directive/parallel/omp_parallel_reduction.cc parallel)
xkomp_add_xfail_test(directive/parallel/omp_parallel_if.cc        parallel)

# --- worksharing : for / sections / single ----------------------------------
xkomp_add_test(directive/worksharing/for/omp_for.cc                 worksharing)
xkomp_add_test(directive/worksharing/for/omp_for_lastprivate.cc     worksharing)
xkomp_add_test(directive/worksharing/for/omp_for_nowait.cc          worksharing)
xkomp_add_test(directive/worksharing/for/omp_for_unsigned.cc        worksharing)
xkomp_add_test(directive/worksharing/for/omp_for_increments.cc      worksharing)
xkomp_add_test(directive/worksharing/for/omp_parallel_for.cc        worksharing)
xkomp_add_xfail_test(directive/worksharing/for/omp_for_schedule_dynamic.cc worksharing)
xkomp_add_xfail_test(directive/worksharing/for/omp_for_static_chunk.cc      worksharing)
xkomp_add_xfail_test(directive/worksharing/for/omp_for_collapse.cc          worksharing)
xkomp_add_xfail_test(directive/worksharing/for/omp_for_int64.cc             worksharing)
xkomp_add_xfail_test(directive/worksharing/for/omp_for_reduction.cc         worksharing)

xkomp_add_xfail_test(directive/worksharing/sections/omp_sections.cc         worksharing)

xkomp_add_test(directive/worksharing/single/omp_single.cc           worksharing)
xkomp_add_test(directive/worksharing/single/omp_single_nowait.cc    worksharing)
xkomp_add_xfail_test(directive/worksharing/single/omp_single_copyprivate.cc worksharing)

# --- master / masked --------------------------------------------------------
xkomp_add_test(directive/master/omp_master.cc                   master)
xkomp_add_xfail_test(directive/masked/omp_masked.cc             masked)

# --- barrier / atomic / critical --------------------------------------------
xkomp_add_test(directive/barrier/omp_barrier.cc                 barrier)
xkomp_add_test(directive/atomic/omp_atomic.cc                   atomic)
xkomp_add_xfail_test(directive/critical/omp_critical.cc         critical)

# --- tasking ----------------------------------------------------------------
xkomp_add_test(directive/tasking/omp_task.cc                    tasking)
xkomp_add_test(directive/tasking/omp_taskwait.cc               tasking)
xkomp_add_test(directive/tasking/omp_task_depend.cc            tasking)
xkomp_add_test(directive/tasking/omp_task_depend_inoutset.cc   tasking)
xkomp_add_test(directive/tasking/omp_task_depend_mutexinoutset.cc tasking)
xkomp_add_test(directive/tasking/omp_task_dag.cc               tasking)
xkomp_add_test(directive/tasking/omp_task_axpy.cc              tasking)
xkomp_add_test(directive/tasking/omp_task_nested.cc            tasking)
xkomp_add_test(directive/tasking/omp_parallel_tasks.cc         tasking)
xkomp_add_test(directive/tasking/omp_task_untied.cc            tasking)
xkomp_add_xfail_test(directive/tasking/omp_taskyield.cc        tasking)
xkomp_add_xfail_test(directive/tasking/omp_taskgroup.cc        tasking)
xkomp_add_xfail_test(directive/tasking/omp_task_if.cc          tasking)

# --- teams (host) -- not implemented ----------------------------------------
xkomp_add_xfail_test(directive/teams/omp_teams.cc              teams)

# --- taskgraph (directive form) -- `#pragma omp taskgraph` not in the compiler
#     yet (the xkomp/ tests cover the same feature via pragma_omp_taskgraph)
xkomp_add_xfail_test(directive/taskgraph/omp_taskgraph.cc      taskgraph)

# --- composite : several constructs combined --------------------------------
xkomp_add_test(directive/composite/omp_nested_worksharing.cc   composite)
xkomp_add_test(directive/composite/omp_manual_reduction.cc     composite)
xkomp_add_test(directive/composite/omp_producer_consumer.cc    composite)
xkomp_add_test(directive/composite/omp_parallel_matvec.cc      composite)

# --- target -- needs the offload runtime, hence guarded ---------------------
if(XKOMP_TESTS_TARGET)
    xkomp_add_test(directive/target/omp_target_map.cc          target)
    xkomp_add_test(directive/target/omp_target_update.cc       target)
    xkomp_add_test(directive/target/omp_target_data.cc         target)
    xkomp_add_test(directive/target/omp_target_axpy.cc         target)
    xkomp_add_test(directive/target/omp_target_axpy_deps.cc    target)
endif()

# --- access clause (XKOMP extension) -- needs the patched LLVM/clang ---------
if(XKOMP_TESTS_ACCESS)
    xkomp_add_test(directive/access/omp_access.cc              access)
    xkomp_add_test(directive/access/omp_access_regions.cc      access)
    xkomp_add_test(directive/access/omp_access_concurrent.cc   access)
    xkomp_add_test(directive/access/omp_access_war_waw.cc      access)
    xkomp_add_test(directive/access/omp_access_dag.cc          access)
    # target + access: also needs the offload runtime
    if(XKOMP_TESTS_TARGET)
        xkomp_add_test(directive/access/omp_access_target.cc          access)
        xkomp_add_test(directive/access/omp_access_target_overlap.cc  access)
        xkomp_add_test(directive/access/omp_access_stencil_target.cc  access)
    endif()
endif()

##############################################################################
# xkomp/ : tests of the xkomp++.h C++ interfaces                            #
##############################################################################

# --- taskgraph (pragma_omp_taskgraph) ---------------------------------------
xkomp_add_test(xkomp/taskgraph/omp_taskgraph.cc                 taskgraph)
xkomp_add_test(xkomp/taskgraph/omp_taskgraph_deps.cc            taskgraph)
# CPU-tasking taskgraphs (no target / offload)
xkomp_add_test(xkomp/taskgraph/omp_taskgraph_axpy_cpu_local.cc  taskgraph)
xkomp_add_test(xkomp/taskgraph/omp_taskgraph_axpy_cpu_global.cc taskgraph)
xkomp_add_test(xkomp/taskgraph/omp_taskgraph_axpby_cpu.cc       taskgraph)
# target/offload taskgraphs
if(XKOMP_TESTS_TARGET)
    xkomp_add_test(xkomp/taskgraph/omp_taskgraph_axpy.cc        taskgraph)
    xkomp_add_test(xkomp/taskgraph/omp_taskgraph_axpby.cc       taskgraph)
endif()
