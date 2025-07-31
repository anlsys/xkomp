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

configure_file(
    ${CMAKE_SOURCE_DIR}/include/xkomp/support.h.in
    ${CMAKE_SOURCE_DIR}/include/xkomp/support.h
)

###############
# C++ version #
###############
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Optional: disables compiler-specific extensions

# if target is enabled, to avoid compilation error due to missing llvm install feature
if (SUPPORT_TARGET)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

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
include_directories(include/)
include_directories(include/xkomp)
include_directories(src/)

install(TARGETS ${XKOMP}                              LIBRARY DESTINATION lib)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libomp.so           DESTINATION lib)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libiomp5.so         DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

###############
# DEPENDENCES #
###############

# XK Runtime
find_package(XKRT REQUIRED)
include_directories("${XKRT_INCLUDE_DIRS}")
target_link_libraries(${XKOMP} PRIVATE ${XKRT_LIBRARIES})

# Target requires an LLVM install
if (SUPPORT_TARGET)
    find_package(LLVM REQUIRED CONFIG)
    include_directories(${LLVM_INCLUDE_DIRS})
    add_definitions(${LLVM_DEFINITIONS})

    # Link to desired LLVM components
    llvm_map_components_to_libnames(LLVM_LIBS
        Core
        Object
        # ObjectOffload
        Support
        IRReader
    )

    target_link_libraries(${XKOMP} PRIVATE ${LLVM_LIBS})

endif()

################
# Sub projects #
################
add_subdirectory(tests/)
enable_testing()
