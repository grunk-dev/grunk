# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

if (NOT SKBUILD)
  message(WARNING "\
  This CMake file is meant to be executed using 'scikit-build'. Running
  it directly will almost certainly not produce the desired result. If
  you are a user trying to install this package, please use the command
  below, which will install all necessary build dependencies, compile
  the package in an isolated environment, and then install it.
  =====================================================================
   $ pip install .
  =====================================================================
  If you are a software developer, and this is your own package, then
  it is usually much more efficient to install the build dependencies
  in your environment once and use the following command that avoids
  a costly creation of a new virtual environment at every compilation:
  =====================================================================
   $ pip install nanobind scikit-build-core[pyproject]
   $ pip install --no-build-isolation -ve .
  =====================================================================
  You may optionally add -Ceditable.rebuild=true to auto-rebuild when
  the package is imported. Otherwise, you need to re-run the above
  after editing C++ files.")
endif()

if (CMAKE_VERSION VERSION_LESS 3.18)
    set(DEV_MODULE Development)
else()
    set(DEV_MODULE Development.Module)
endif()

find_package(Python 3.9 COMPONENTS Interpreter ${DEV_MODULE} REQUIRED)

# scikit-build-core (the supported `pip install`/SKBUILD path) injects nanobind_DIR itself before
# this file is even processed. A bare cmake/ccmake configure with GRUNK_PYTHON toggled on manually
# has no such injection, so find_package(nanobind) below would otherwise only succeed by accident
# (a stale nanobind_DIR left over in CMakeCache.txt from an earlier SKBUILD-driven configure of the
# same build directory). Query nanobind's own CMake dir from the selected interpreter so both paths
# work identically.
if (NOT DEFINED nanobind_DIR)
    execute_process(
        COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
        OUTPUT_VARIABLE NB_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    list(APPEND CMAKE_PREFIX_PATH "${NB_DIR}")
endif()
find_package(nanobind CONFIG REQUIRED)

# We are now ready to compile the actual extension module
nanobind_add_module(
  # Name of the extension
  _bindings

  # Target the stable ABI for Python 3.12+, which reduces
  # the number of binary wheels that must be built. This
  # does nothing on older Python versions
  STABLE_ABI

  # Build libnanobind statically and merge it into the
  # extension (which itself remains a shared library)
  #
  # If your project builds multiple extensions, you can
  # replace this flag by NB_SHARED to conserve space by
  # reusing a shared libnanobind across libraries
  NB_STATIC

  # Source code goes here
  _bindings.cpp
)
target_link_libraries(_bindings PRIVATE grunk)

# Install directive for scikit-build-core
install(TARGETS _bindings LIBRARY DESTINATION grunk)

# grunk's own shared libraries (grunk/grunk_recipe/grunk_plugin, all SHARED - see
# ../CMakeLists.txt's own RPATH comment) must ship *inside* the Python package
# directory too, alongside _bindings itself - a Python wheel has no general "shared
# library dependency" mechanism of its own, so _bindings' own $ORIGIN-relative
# install RPATH (set globally in ../CMakeLists.txt) can only find them if they're
# co-located in the exact same directory _bindings is installed into. This is what
# lets a `pip install`-built grunk wheel dynamically link its own bundled C++ core
# instead of statically embedding a private copy - the same single-shared-copy
# property any *other* native plugin loaded into the same process (grunk-occt,
# grunk-adolc, ...) needs, for the reasons ../CMakeLists.txt's own comment explains.
foreach (grunk_shared_target grunk grunk_recipe grunk_plugin)
    if (TARGET ${grunk_shared_target})
        install(TARGETS ${grunk_shared_target}
            LIBRARY DESTINATION grunk
            RUNTIME DESTINATION grunk
        )
    endif()
endforeach()