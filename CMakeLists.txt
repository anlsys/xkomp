# CMake Minimum version
cmake_minimum_required(VERSION 3.20)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

SET(XKOMP xkomp)

# Project name
project(${XKOMP})

# Version
set(VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/version.map")

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
    ${CMAKE_SOURCE_DIR}/src/target/api.cc
    ${CMAKE_SOURCE_DIR}/src/task.cc
)

#########
# XKOMP #
#########

# XKOMP Lib
add_custom_target( omp  ALL COMMAND ${CMAKE_COMMAND} -E create_symlink lib${XKOMP}.so libomp.so)
add_custom_target(iomp5 ALL COMMAND ${CMAKE_COMMAND} -E create_symlink lib${XKOMP}.so libiomp5.so)
add_library(${XKOMP}  SHARED ${SOURCE_FILES})

# flags
if(UNIX AND NOT APPLE)
    target_link_options(${XKOMP}        PRIVATE "-Wl,--version-script=${VERSION_SCRIPT}")
endif()

# Include directory
include_directories(include/)
include_directories(include/xkomp)
include_directories(src/)

install(FILES   omp iomp5          DESTINATION lib)

###############
# DEPENDENCES #
###############

# XK Runtime
find_package(XKRT REQUIRED)
include_directories("${XKRT_INCLUDE_DIRS}")
target_link_libraries(${XKOMP} PRIVATE ${XKRT_LIBRARIES})

# # LLVM includes
# find_package(LLVM REQUIRED)
# include_directories("${LLVM_INCLUDE_DIRS}/llvm/include")
# include_directories("${LLVM_INCLUDE_DIRS}/offload/include")

################
# Sub projects #
################
add_subdirectory(tests/)
enable_testing()
