# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.15)

project(grunk VERSION 0.5.0)

include(GNUInstallDirs)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

option(GRUNK_WITH_TASKFLOW "Builds grunk with multi-threading support using Taskflow" ON)
option(GRUNK_WITH_DYNAMIC  "Builds the grunk::dynamic library, for dynamic parametric models" ON)
option(GRUNK_WITH_RECIPE   "Builds the grunk::recipe library, for exporting and importing recipes from/to yaml format" ON)
option(GRUNK_PYTHON        "Build python bindings using pybind11" OFF)
option(GRUNK_TESTS         "Build unit tests using GTest" OFF)

# If scikit-build (SKBUILD) is driving the build (i.e. when building wheels),
# enable Python bindings by default so the `python/` subdirectory is
# processed and the nanobind extension is built and packaged.
if(DEFINED SKBUILD)
    message(STATUS "SKBUILD detected: enabling Python bindings (GRUNK_PYTHON=ON)")
    set(GRUNK_PYTHON ON CACHE BOOL "Build python bindings using nanobind" FORCE)
endif()

# When building Python bindings (or when SKBUILD drives the build) we may
# need static libraries to be compiled as position-independent so they can
# be linked into the Python extension module. Enable PIC in that case.
if(DEFINED SKBUILD OR GRUNK_PYTHON)
    message(STATUS "Enabling position independent code for building Python extension modules")
    # This causes targets (including STATIC libraries) to be built with -fPIC
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()


# conan creates FindPackage.cmake files in the CMAKE_BINARY_DIR
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})

# find dependencies
find_package(parametric REQUIRED)

if (GRUNK_WITH_TASKFLOW)
    find_package(Taskflow REQUIRED)
endif()

if (GRUNK_WITH_DYNAMIC)
    find_package(sol2 REQUIRED)
    find_package(Lua REQUIRED)
endif()

if (GRUNK_WITH_RECIPE)
    find_package(yaml-cpp REQUIRED)
endif()

# Windows specific: export all symbols
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_subdirectory(grunk)

if (GRUNK_TESTS)
    add_subdirectory(tests/cpp)
endif()

if (GRUNK_PYTHON)
    add_subdirectory(python)
endif()

# Create a package configuration file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/grunkConfigVersion.cmake"
    VERSION grunk_VERSION
    COMPATIBILITY AnyNewerVersion
)

install(
    EXPORT grunkTargets
    FILE grunkTargets.cmake
    NAMESPACE grunk::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/grunk
)

configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/grunkConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/grunkConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/grunk
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/grunkConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/grunkConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/grunk
)
