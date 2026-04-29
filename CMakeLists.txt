# CMake Minimum version
cmake_minimum_required(VERSION 3.20)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

SET(XKOMP xkomp)

# Project name
project(${XKOMP} C CXX ASM)

###########
# OPTIONS #
###########

include(BooleanUtils)
xkoption(SUPPORT_TARGET "Build support for target constructs, which requires LLVM." ON)
xkoption(SUPPORT_MICROTASK_GENERIC "Use generic implem for kmp microtasks (limited in number of arguments)." OFF)
xkoption(SUPPORT_MICROTASK_LIBFFI "Use libffi for kmp microtasks (experimental)." OFF)
xkoption(SUPPORT_MICROTASK_INTEL "Use Intel x86 implementation for kmp microtasks." ON)

###############
# C++ version #
###############

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Optional: disables compiler-specific extensions

#########
# XKOMP #
#########

SET(SOURCE_FILES

    # COMMON STANDARD RUNTIME API
    ${CMAKE_SOURCE_DIR}/src/api.cc
    ${CMAKE_SOURCE_DIR}/src/device.cc
    ${CMAKE_SOURCE_DIR}/src/env.cc

    # GOMP
    ${CMAKE_SOURCE_DIR}/src/gomp/task.cc
    ${CMAKE_SOURCE_DIR}/src/gomp/parallel.cc

    # KMP
    ${CMAKE_SOURCE_DIR}/src/kmp/barrier.cc
    ${CMAKE_SOURCE_DIR}/src/kmp/for.cc
    ${CMAKE_SOURCE_DIR}/src/kmp/fork.cc
    ${CMAKE_SOURCE_DIR}/src/kmp/master.cc
    ${CMAKE_SOURCE_DIR}/src/kmp/single.cc
    ${CMAKE_SOURCE_DIR}/src/kmp/thread.cc
    ${CMAKE_SOURCE_DIR}/src/kmp/task.cc

    # XKOMP
    ${CMAKE_SOURCE_DIR}/src/xkomp/taskgraph.cc
)

# if target is enabled, add sources
if (SUPPORT_TARGET)
    list(APPEND SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/kmp/target/api.cc)
endif()

# Microtask impl to use
set(enabled_count 0)

if (SUPPORT_MICROTASK_GENERIC)
    list(APPEND SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/kmp/arch/generic.cc)
    math(EXPR enabled_count "${enabled_count} + 1")
endif()

if (SUPPORT_MICROTASK_LIBFFI)
    list(APPEND SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/kmp/arch/libffi.cc)
    math(EXPR enabled_count "${enabled_count} + 1")
endif()

if (SUPPORT_MICROTASK_INTEL)
    list(APPEND SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/kmp/arch/z_Linux_asm.S)
    add_compile_definitions(KMP_ARCH_X86_64=1)
    math(EXPR enabled_count "${enabled_count} + 1")
endif()

if(NOT enabled_count EQUAL 1)
    message(FATAL_ERROR "Invalid configuration: Exactly one of  56 if (SUPPORT_MICROTASK_GENERIC)
, SUPPORT_MICROTASK_LIBFFI, or SUPPORT_MICROTASK_INTEL must be ON. Currently, ${enabled_count} are enabled.")
endif()

# XKOMP Lib
# Define the actual files as outputs
add_custom_command(
    OUTPUT libomp.so libiomp5.so
    COMMAND ${CMAKE_COMMAND} -E create_symlink lib${XKOMP}.so libomp.so
    COMMAND ${CMAKE_COMMAND} -E create_symlink lib${XKOMP}.so libiomp5.so
    COMMAND ${CMAKE_COMMAND} -E create_symlink lib${XKOMP}.so libgomp.so
    COMMAND ${CMAKE_COMMAND} -E create_symlink lib${XKOMP}.so libgomp.so.1
    DEPENDS ${XKOMP}
)
add_custom_target(compat_links ALL DEPENDS libomp.so libiomp5.so)

add_library(${XKOMP} SHARED ${SOURCE_FILES})

# flags
if(UNIX AND NOT APPLE)
    target_link_options(${XKOMP} PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/abi.map")
    target_link_options(${XKOMP} PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/gomp/abi.map")
    target_link_options(${XKOMP} PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/kmp/abi.map")
    target_link_options(${XKOMP} PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/xkomp/abi.map")
endif()

# Include directory
include_directories(${CMAKE_SOURCE_DIR}/include/)
include_directories(${CMAKE_BINARY_DIR}/include/)
include_directories(${CMAKE_SOURCE_DIR}/src/)

###############
# DEPENDENCES #
###############

# XK Runtime
find_package(XKRT REQUIRED NO_MODULE)
target_link_libraries(${XKOMP} PRIVATE XKRT::xkrt)

# libffi
if (SUPPORT_MICROTASK_LIBFFI)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LibFFI REQUIRED libffi)
    target_include_directories(${XKOMP} PRIVATE ${LibFFI_INCLUDE_DIRS})
    target_link_libraries(${XKOMP} PRIVATE ${LibFFI_LIBRARIES})
endif()

################
# INSTALLATION #
################

# Header file generated
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include/xkomp")
configure_file(
    ${CMAKE_SOURCE_DIR}/include/xkomp/support.h.in
    ${CMAKE_BINARY_DIR}/include/xkomp/support.h
)
install(
    FILES "${CMAKE_BINARY_DIR}/include/xkomp/support.h"
    DESTINATION include/xkomp
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

# install lib
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libomp.so
    ${CMAKE_CURRENT_BINARY_DIR}/libiomp5.so
    DESTINATION lib
)
install(TARGETS ${XKOMP} LIBRARY DESTINATION lib)

# Installation - cmake files
target_include_directories(${XKOMP} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
install(TARGETS ${XKOMP}
    EXPORT XKOMPTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT XKOMPTargets
    FILE XKOMPTargets.cmake
    NAMESPACE XKOMP::
    DESTINATION lib/cmake/XKOMP)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/XKOMPConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/XKOMPConfig.cmake"
    INSTALL_DESTINATION lib/cmake/XKOMP
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/XKOMPConfig.cmake"
    DESTINATION lib/cmake/XKOMP
)

################
# Sub projects #
################
add_subdirectory(tests/)
enable_testing()
