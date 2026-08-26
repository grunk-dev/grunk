# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.10)
project(grunk-tests)

if (NOT TARGET grunk)

  find_package(grunk REQUIRED COMPONENTS core)

  if (GRUNK_WITH_DYNAMIC)
    find_package(grunk REQUIRED COMPONENTS dynamic)
  endif()

  if (GRUNK_WITH_RECIPE)
      find_package(grunk REQUIRED COMPONENTS recipe)
  endif()

  if (GRUNK_WITH_PLUGIN)
      find_package(grunk REQUIRED COMPONENTS plugin)
  endif()

endif()

enable_testing()
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(GOOGLE_TEST_INDIVIDUAL ON)
include(AddGoogleTest)

set(TESTSOURCES testmain.cpp)

if (GRUNK_WITH_TASKFLOW)
    list(APPEND TESTSOURCES core_multithreading.cpp)
endif()

if (GRUNK_WITH_DYNAMIC)
    list(APPEND TESTSOURCES
      dynamic_operators.cpp
      dynamic_serialization.cpp
      dynamic_state.cpp
      dynamic_std_vector.cpp
      dynamic_module.cpp
      dynamic_plugin.cpp
      plugin_begin.cpp
      plugin_lua_script.cpp
    )
endif()

if (GRUNK_WITH_RECIPE)
    list(APPEND TESTSOURCES
      recipe_simple.cpp
      recipe_clone.cpp
      recipe_call.cpp
      recipe_module.cpp
      recipe_plugin.cpp
    )
endif()

if (GRUNK_WITH_PLUGIN)
    list(APPEND TESTSOURCES plugin_loader.cpp)

    # A real shared library for plugin_loader.cpp to dlopen via grunk::plugin::load_native
    # - every other plugin test uses an in-process function/lua_CFunction stand-in instead
    # (see dynamic_plugin.cpp's luaopen_testplugin), since only the loader itself needs an
    # actual .so on disk.
    add_library(plugin_native_fixture SHARED plugin_native_fixture.cpp)
    # Linking grunk::plugin (not just grunk::dynamic) matches what a real plugin is
    # documented to do (docs/usage.rst's "Writing Plugins") - see grunk/plugin/CMakeLists.txt
    # for why plain grunk::dynamic isn't enough once a grunk::state is actually
    # instantiated with GRUNK_WITH_RECIPE on.
    target_link_libraries(plugin_native_fixture PRIVATE grunk::plugin)

    # A second fixture whose grunk_plugin_register deliberately throws, so
    # plugin_loader.cpp can verify load_native rolls back a half-registered plugin's
    # identity instead of reporting it as loaded (see plugin_native_fixture_failing.cpp).
    add_library(plugin_native_fixture_failing SHARED plugin_native_fixture_failing.cpp)
    target_link_libraries(plugin_native_fixture_failing PRIVATE grunk::plugin)
endif()

add_executable(runUnitTests ${TESTSOURCES})
target_link_libraries(runUnitTests PRIVATE grunk gtest_main)

if (GRUNK_WITH_PLUGIN)
    target_compile_definitions(runUnitTests PRIVATE
        NATIVE_FIXTURE_SO_PATH="$<TARGET_FILE:plugin_native_fixture>"
        FAILING_NATIVE_FIXTURE_SO_PATH="$<TARGET_FILE:plugin_native_fixture_failing>"
    )
    add_dependencies(runUnitTests plugin_native_fixture plugin_native_fixture_failing)
endif()

