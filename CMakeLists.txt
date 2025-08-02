# CMake Minimum version
cmake_minimum_required(VERSION 3.20)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

SET(XKOMP xkomp)

# Project name
project(${XKOMP})

# Version
set(VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/version.map")

###########
# OPTIONS #
###########

option(SUPPORT_TARGET "Build support for target constructs, which requires LLVM." ON)
include(BooleanUtils)
convert_bool(SUPPORT_TARGET)

###############
# C++ version #
###############
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Optional: disables compiler-specific extensions

###############
# DEPENDENCES #
###############

SET(SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/api.cc
    ${CMAKE_SOURCE_DIR}/src/barrier.cc
    ${CMAKE_SOURCE_DIR}/src/device.cc
    ${CMAKE_SOURCE_DIR}/src/env.cc
    ${CMAKE_SOURCE_DIR}/src/for.cc
    ${CMAKE_SOURCE_DIR}/src/fork.cc
    ${CMAKE_SOURCE_DIR}/src/master.cc
    ${CMAKE_SOURCE_DIR}/src/single.cc
    ${CMAKE_SOURCE_DIR}/src/task.cc
)

# if target is enabled, add sources
if (SUPPORT_TARGET)
    list(APPEND SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/target/api.cc)
endif()

#########
# XKOMP #
#########

# XKOMP Lib
add_custom_target( omp  ALL COMMAND ${CMAKE_COMMAND} -E create_symlink lib${XKOMP}.so libomp.so)
add_custom_target(iomp5 ALL COMMAND ${CMAKE_COMMAND} -E create_symlink lib${XKOMP}.so libiomp5.so)
add_library(${XKOMP}  SHARED ${SOURCE_FILES})

# flags
if(UNIX AND NOT APPLE)
    target_link_options(${XKOMP} PRIVATE "-Wl,--version-script=${VERSION_SCRIPT}")
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

# install lib
install(TARGETS ${XKOMP}                              LIBRARY DESTINATION lib)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libomp.so           DESTINATION lib)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libiomp5.so         DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

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
