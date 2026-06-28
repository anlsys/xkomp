cmake_minimum_required(VERSION 3.20)
include(CTest)

project(tests CXX)

#
# XKOMP test-suite
# ----------------
# Organised by construct, mirroring the LLVM OpenMP runtime test layout:
#
#   parallel/            worksharing/for/      worksharing/sections/
#   worksharing/single/  master/   masked/     barrier/   atomic/   critical/
#   tasking/             taskgraph/            target/     teams/
#   composite/           (several constructs combined)
#
# Each test documents its status at the top of the file:
#   // xkomp: supported          -> expected to PASS
#   // xkomp: XFAIL <reason>      -> construct not implemented yet (see below)
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
    # The taskgraph tests include <xkrt/*.h>; link the imported target so its
    # include directories propagate (xkomp links XKRT privately, so it does not
    # carry them transitively).  Fall back to the plain variables otherwise.
    if(TARGET XKRT::xkrt)
        target_link_libraries(${tgt} PRIVATE XKRT::xkrt)
    else()
        target_link_libraries(${tgt} PRIVATE ${XKRT_LIBRARIES})
        target_include_directories(${tgt} PRIVATE ${XKRT_INCLUDE_DIRS})
    endif()
endfunction()

# Expected-to-PASS test.  usage: xkomp_add_test(<name> <source> <label>)
function(xkomp_add_test name src label)
    add_executable(${name} ${src})
    _xkomp_test_setup(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS "${label}")
endfunction()

# Expected-to-FAIL test (construct not implemented yet).  Built on demand and
# driven through run_xfail.cmake; marked WILL_FAIL so it is green while the
# construct is missing and turns red once it works (-> promote it).
function(xkomp_add_xfail_test name src label)
    add_executable(${name} EXCLUDE_FROM_ALL ${src})
    _xkomp_test_setup(${name})
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            -DTARGET=${name}
            -DBIN_DIR=${CMAKE_BINARY_DIR}
            -DEXE=$<TARGET_FILE:${name}>
            -P ${CMAKE_SOURCE_DIR}/cmake/run_xfail.cmake)
    set_tests_properties(${name} PROPERTIES WILL_FAIL TRUE LABELS "xfail;${label}")
endfunction()

##############################################################################
# parallel                                                                    #
##############################################################################
xkomp_add_test(parallel               parallel/omp_parallel.cc               parallel)
xkomp_add_test(parallel_num_threads   parallel/omp_parallel_num_threads.cc   parallel)
xkomp_add_test(parallel_firstprivate  parallel/omp_parallel_firstprivate.cc  parallel)
xkomp_add_test(parallel_nested        parallel/omp_parallel_nested.cc        parallel)
xkomp_add_xfail_test(parallel_reduction parallel/omp_parallel_reduction.cc   parallel)
xkomp_add_xfail_test(parallel_if      parallel/omp_parallel_if.cc            parallel)

##############################################################################
# worksharing : for / sections / single                                       #
##############################################################################
xkomp_add_test(for                    worksharing/for/omp_for.cc                 worksharing)
xkomp_add_test(for_lastprivate        worksharing/for/omp_for_lastprivate.cc     worksharing)
xkomp_add_test(for_nowait             worksharing/for/omp_for_nowait.cc          worksharing)
xkomp_add_test(for_unsigned           worksharing/for/omp_for_unsigned.cc        worksharing)
xkomp_add_test(for_increments         worksharing/for/omp_for_increments.cc      worksharing)
xkomp_add_test(parallel_for           worksharing/for/omp_parallel_for.cc        worksharing)
xkomp_add_xfail_test(for_schedule_dynamic worksharing/for/omp_for_schedule_dynamic.cc worksharing)
xkomp_add_xfail_test(for_static_chunk worksharing/for/omp_for_static_chunk.cc    worksharing)
xkomp_add_xfail_test(for_collapse     worksharing/for/omp_for_collapse.cc        worksharing)
xkomp_add_xfail_test(for_int64        worksharing/for/omp_for_int64.cc           worksharing)
xkomp_add_xfail_test(for_reduction    worksharing/for/omp_for_reduction.cc       worksharing)

xkomp_add_xfail_test(sections         worksharing/sections/omp_sections.cc       worksharing)

xkomp_add_test(single                 worksharing/single/omp_single.cc           worksharing)
xkomp_add_test(single_nowait          worksharing/single/omp_single_nowait.cc    worksharing)
xkomp_add_xfail_test(single_copyprivate worksharing/single/omp_single_copyprivate.cc worksharing)

##############################################################################
# master / masked                                                             #
##############################################################################
xkomp_add_test(master                 master/omp_master.cc                   master)
xkomp_add_xfail_test(masked           masked/omp_masked.cc                   masked)

##############################################################################
# barrier / atomic / critical                                                 #
##############################################################################
xkomp_add_test(barrier                barrier/omp_barrier.cc                 barrier)
xkomp_add_test(atomic                 atomic/omp_atomic.cc                   atomic)
xkomp_add_xfail_test(critical         critical/omp_critical.cc               critical)

##############################################################################
# tasking                                                                     #
##############################################################################
xkomp_add_test(task                   tasking/omp_task.cc                    tasking)
xkomp_add_test(taskwait               tasking/omp_taskwait.cc                tasking)
xkomp_add_test(task_depend            tasking/omp_task_depend.cc             tasking)
xkomp_add_test(task_depend_inoutset   tasking/omp_task_depend_inoutset.cc    tasking)
xkomp_add_test(task_depend_mutexinoutset tasking/omp_task_depend_mutexinoutset.cc tasking)
xkomp_add_test(task_dag               tasking/omp_task_dag.cc                tasking)
xkomp_add_test(task_nested            tasking/omp_task_nested.cc             tasking)
xkomp_add_test(parallel_tasks         tasking/omp_parallel_tasks.cc          tasking)
xkomp_add_test(task_untied            tasking/omp_task_untied.cc             tasking)
xkomp_add_xfail_test(taskyield        tasking/omp_taskyield.cc               tasking)
xkomp_add_xfail_test(taskgroup        tasking/omp_taskgroup.cc               tasking)
xkomp_add_xfail_test(task_if          tasking/omp_task_if.cc                 tasking)

##############################################################################
# taskgraph (XKOMP extension)                                                 #
##############################################################################
xkomp_add_test(taskgraph              taskgraph/omp_taskgraph.cc             taskgraph)
xkomp_add_test(taskgraph_deps         taskgraph/omp_taskgraph_deps.cc        taskgraph)

##############################################################################
# teams (host) -- not implemented                                             #
##############################################################################
xkomp_add_xfail_test(teams            teams/omp_teams.cc                     teams)

##############################################################################
# composite : several constructs combined into compliant patterns             #
##############################################################################
xkomp_add_test(nested_worksharing     composite/omp_nested_worksharing.cc    composite)
xkomp_add_test(manual_reduction       composite/omp_manual_reduction.cc      composite)
xkomp_add_test(producer_consumer      composite/omp_producer_consumer.cc     composite)
xkomp_add_test(parallel_matvec        composite/omp_parallel_matvec.cc       composite)

##############################################################################
# target -- need the offload runtime, hence guarded by an option              #
##############################################################################
if(XKOMP_TESTS_TARGET)
    xkomp_add_test(target_map         target/omp_target_map.cc               target)
    xkomp_add_test(target_update      target/omp_target_update.cc            target)
    xkomp_add_test(target_data        target/omp_target_data.cc              target)
    xkomp_add_test(taskgraph_axpy     taskgraph/omp_taskgraph_axpy.cc        target)
endif()

##############################################################################
# access clause (XKOMP extension) -- needs the patched LLVM/clang             #
##############################################################################
if(XKOMP_TESTS_ACCESS)
    xkomp_add_test(access             access/omp_access.cc             access)
    xkomp_add_test(access_regions     access/omp_access_regions.cc     access)
    xkomp_add_test(access_war_waw     access/omp_access_war_waw.cc     access)
    xkomp_add_test(access_dag         access/omp_access_dag.cc         access)
    # target + access: also needs the offload runtime
    if(XKOMP_TESTS_TARGET)
        xkomp_add_test(access_target          access/omp_access_target.cc          access)
        xkomp_add_test(access_target_overlap  access/omp_access_target_overlap.cc  access)
        xkomp_add_test(access_stencil_target  access/omp_access_stencil_target.cc  access)
    endif()
endif()
