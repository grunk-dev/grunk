# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.15)
project(cad_autodiff_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_PREFIX_PATH "$ENV{CONDA_PREFIX}")

file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/cmake")

include(FetchContent)

FetchContent_Declare(geoml
    GIT_REPOSITORY https://github.com/DLR-SC/geoml.git
    GIT_TAG main
)

FetchContent_Declare(grunk
    GIT_REPOSITORY https://github.com/grunk-dev/grunk.git
    GIT_TAG cad_ad_example #TODO: Replace with main
)

FetchContent_GetProperties(geoml)
if(NOT geoml_POPULATED)
    cmake_policy(SET CMP0169 OLD)
    FetchContent_Populate(geoml)

    if(EXISTS "${geoml_SOURCE_DIR}/cmake/geomlConfig.cmake.in")
        file(COPY "${geoml_SOURCE_DIR}/cmake/geomlConfig.cmake.in"
             DESTINATION "${CMAKE_SOURCE_DIR}/cmake"
             NO_SOURCE_PERMISSIONS)
    endif()

    add_subdirectory(${geoml_SOURCE_DIR} ${geoml_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

FetchContent_GetProperties(grunk)
if(NOT grunk_POPULATED)
    cmake_policy(SET CMP0169 OLD)
    FetchContent_Populate(grunk)

    if(EXISTS "${grunk_SOURCE_DIR}/cmake/grunkConfig.cmake.in")
        file(COPY "${grunk_SOURCE_DIR}/cmake/grunkConfig.cmake.in"
             DESTINATION "${CMAKE_SOURCE_DIR}/cmake"
             NO_SOURCE_PERMISSIONS)
    endif()

    set(GRUNK_WITH_TASKFLOW OFF CACHE BOOL "" FORCE)
    set(GRUNK_WITH_DYNAMIC ON CACHE BOOL "" FORCE)
    set(GRUNK_WITH_RECIPE ON CACHE BOOL "" FORCE)
    set(GRUNK_TESTS OFF CACHE BOOL "" FORCE)
    set(GRUNK_PYTHON OFF CACHE BOOL "" FORCE)

    include_directories(${grunk_BINARY_DIR})
    add_subdirectory(${grunk_SOURCE_DIR} ${grunk_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

set(ADOLC_BASE_DIR "" CACHE PATH "Path to ADOL-C installation directory")

if(NOT DEFINED ADOLC_INCLUDE_DIR)
  set(ADOLC_INCLUDE_DIR "${ADOLC_BASE_DIR}/include" CACHE PATH "The directory containing adol-c headers")
endif(NOT DEFINED ADOLC_INCLUDE_DIR)
if(NOT DEFINED ADOLC_LIB_DIR)
  set(ADOLC_LIB_DIR "${ADOLC_BASE_DIR}/lib64" CACHE PATH "The directory containing adol-c library")
endif(NOT DEFINED ADOLC_LIB_DIR)

find_file(ADOLC_HEADER_LOCATION "adolc/adtl.h" PATHS ${ADOLC_INCLUDE_DIR})
if(${ADOLC_HEADER_LOCATION} STREQUAL "ADOLC_HEADER_LOCATION-NOTFOUND")
  message(FATAL_ERROR "adolc/adtl.h can not be found, define ADOLC_INCLUDE_DIR")
else(${ADOLC_HEADER_LOCATION} STREQUAL "ADOLC_HEADER_LOCATION-NOTFOUND")
  include_directories(${ADOLC_INCLUDE_DIR})
endif(${ADOLC_HEADER_LOCATION} STREQUAL "ADOLC_HEADER_LOCATION-NOTFOUND")

# Find ADOL-C library
find_library(ADOLC_LIBRARY
    NAMES adolc libadolc
    PATHS "${ADOLC_LIB_DIR}"
    NO_DEFAULT_PATH
)

if(NOT ADOLC_LIBRARY)
    message(FATAL_ERROR "ADOL-C library not found, define ADOLC_LIB_DIR")
endif()

message(STATUS "Found ADOL-C library: ${ADOLC_LIBRARY}")
message(STATUS "ADOL-C include directory: ${ADOLC_INCLUDE_DIR}")

# adtl.so is a SWIG-generated Lua module wrapping ADOL-C's tapeless adouble type,
# built manually from https://gitlab.dlr.de/dlr-sp/occt-differentiation/swig-adol-c.git
# (branch lua_wrapper) - see README.md for build instructions. It is loaded into the
# grunk::state's Lua interpreter at runtime (grunk::state::load_module), so main.cpp
# never includes adolc/adtl.h directly, only ADTL_LIBRARY needs to be linked.
set(ADTL_LIB_DIR "${CMAKE_SOURCE_DIR}/build/_deps/swig-adol-c/build" CACHE PATH "Directory containing the built adtl Lua module (adtl.so)")

# swig_add_library sets PREFIX "" on the adtl target, so the file is literally named
# "adtl.so" with no "lib" prefix - find_library's implicit lib/suffix conventions won't
# match that, so look it up as a plain file instead.
find_file(ADTL_LIBRARY
    NAMES adtl.so
    PATHS "${ADTL_LIB_DIR}"
    NO_DEFAULT_PATH
)

if(NOT ADTL_LIBRARY)
    message(FATAL_ERROR "adtl Lua module (adtl.so) not found, define ADTL_LIB_DIR. See README.md for build instructions.")
endif()

message(STATUS "Found adtl Lua module: ${ADTL_LIBRARY}")

add_executable(cad_autodiff src/main.cpp)
target_link_libraries(cad_autodiff PRIVATE grunk geoml ${ADOLC_LIBRARY} ${ADTL_LIBRARY})
