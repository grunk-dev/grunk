# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

# Stage 3's AD-enabled geoml plugin - see ../../README.md's "Stage 3" section for
# what to build first (adOCCT, then geoml's feature/autodiff branch against it)
# before configuring this.
#
# Separate CMake project from ../../CMakeLists.txt (configured/built on its own - see
# README.md), not a target added there: the main project already claims the CMake
# target name `geoml` for stock geoml (OCCT 7.6.2); this needs a *different*,
# AD-enabled `geoml` (built against adOCCT's OCCT fork) under that same name, which
# find_package(geoml) would collide with in one CMake tree. Mirrors how swig-adol-c
# (../../README.md) is already its own separately-built project, consumed by output
# path only - not CMake target-linking - for the same reason.
cmake_minimum_required(VERSION 3.15)
project(geoml_adolc_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_PREFIX_PATH "$ENV{CONDA_PREFIX}")

file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/cmake")

include(FetchContent)

# grunk itself is OCCT-agnostic (it never names an OCCT type), so there is nothing
# AD-specific about this dependency - fetched the same way ../../CMakeLists.txt does.
FetchContent_Declare(grunk
    GIT_REPOSITORY https://github.com/grunk-dev/grunk.git
    GIT_TAG main
)

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

# The AD-enabled geoml install: geoml's feature/autodiff branch
# (https://github.com/DLR-SC/geoml/tree/feature/autodiff), configured with
# -DGEOML_USE_ADOLC=ON, built against an adOCCT install (not stock OpenCASCADE) - see
# README.md. Not FetchContent'd here (unlike stock geoml in ../../CMakeLists.txt):
# geoml's own OCCT 7.6.2 pin needed reconciling against adOCCT's OCCT V7_9_0 base -
# see patches/geoml-feature-autodiff-occt79.patch (applied to the checkout that
# produced GEOML_AUTODIFF_DIR, not by this CMake project). For now this expects an
# already-built, already-installed geoml.
set(GEOML_AUTODIFF_DIR "" CACHE PATH
    "Install prefix of geoml's feature/autodiff branch, built against adOCCT with GEOML_USE_ADOLC=ON")
if(NOT GEOML_AUTODIFF_DIR)
    message(FATAL_ERROR "GEOML_AUTODIFF_DIR is not set - see README.md's \"Stage 3\" section.")
endif()
list(APPEND CMAKE_PREFIX_PATH "${GEOML_AUTODIFF_DIR}")

# The adOCCT install that GEOML_AUTODIFF_DIR above was built against. geoml's own
# geoml-config.cmake doesn't restore either of the two dependencies below - its
# install exports "geoml" linking against bare OCCT target names (TKernel, TKMath,
# ...) and an ADOLC::ADOLC target, both defined ad hoc by geoml's UseOpenCASCADE.cmake/
# UseADOLC.cmake (find_package(OpenCASCADE)/pkg_check_modules, not a find_dependency
# geoml's install can replay for us) - so any consumer of the installed geoml has to
# recreate both here first, or the bare names resolve to nothing and every OCCT
# header/symbol geoml_registration.hpp needs goes missing.
set(ADOCCT_DIR "" CACHE PATH
    "Install prefix of adOCCT (dlr-sp/adOCCT, ad/master) that GEOML_AUTODIFF_DIR was built against")
if(NOT ADOCCT_DIR)
    message(FATAL_ERROR "ADOCCT_DIR is not set - see README.md's \"Stage 3\" section.")
endif()
# NO_DEFAULT_PATH + explicit PATHS, not CMAKE_PREFIX_PATH: CONDA_PREFIX (line 22,
# needed for Lua/sol2) also has its own stock opencascade==7.6.2 conda package
# installed, and its OpenCASCADEConfig.cmake would otherwise be found ahead of
# adOCCT's - silently linking the AD-enabled geoml against the wrong, non-AD OCCT.
find_package(OpenCASCADE CONFIG REQUIRED PATHS "${ADOCCT_DIR}" NO_DEFAULT_PATH)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ADOLC REQUIRED IMPORTED_TARGET GLOBAL adolc)
add_library(ADOLC::ADOLC INTERFACE IMPORTED GLOBAL)
target_link_libraries(ADOLC::ADOLC INTERFACE PkgConfig::ADOLC)

find_package(geoml CONFIG REQUIRED)

add_library(geoml_adolc_plugin SHARED geoml_adolc_plugin.cpp)
target_link_libraries(geoml_adolc_plugin PRIVATE grunk geoml)
