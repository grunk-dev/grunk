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

    # A third fixture whose registration function throws a non-std::exception value,
    # so plugin_loader.cpp can verify GRUNK_PLUGIN_REGISTER's catch(...) branch reports
    # it cleanly instead of letting it escape (see plugin_native_fixture_nonstd_throw.cpp).
    add_library(plugin_native_fixture_nonstd_throw SHARED plugin_native_fixture_nonstd_throw.cpp)
    target_link_libraries(plugin_native_fixture_nonstd_throw PRIVATE grunk::plugin)

    # Windows only - see the identical, more detailed comment on runUnitTests' own
    # copy step below. These three are loaded via grunk::plugin::load_native from an
    # *absolute path* ($<TARGET_FILE:...>, baked in as a compile definition below),
    # not linked - Windows searches a DLL's own directory first for *its* dependencies
    # when loaded this way, so each fixture needs grunk's DLLs next to itself too, not
    # just next to runUnitTests.exe (belt and braces: they already land in the same
    # directory as runUnitTests.exe by CMake's own default per-CMakeLists.txt output
    # layout, but relying on that implicitly here would be fragile).
    if (WIN32)
        foreach(_fixture plugin_native_fixture plugin_native_fixture_failing plugin_native_fixture_nonstd_throw)
            add_custom_command(TARGET ${_fixture} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:${_fixture}>
                    $<TARGET_FILE_DIR:${_fixture}>
                COMMAND_EXPAND_LISTS
            )
        endforeach()
    endif()
endif()

add_executable(runUnitTests ${TESTSOURCES})
target_link_libraries(runUnitTests PRIVATE grunk gtest_main)

# Windows has no RPATH/RUNPATH mechanism - grunk (now SHARED - see ../CMakeLists.txt's
# own comment) has to actually be *found* by runUnitTests.exe at test time, before
# `cmake --install` ever runs (pixi's own test_cpp task runs the exe straight out of
# the build tree - see pyproject.toml). On Linux/macOS this "just works" via CMake's
# own default build-tree RPATH; on Windows the loader only ever looks in the exe's own
# directory (plus PATH, which only covers *conda-installed* DLLs like Lua's, already on
# PATH via pixi's own activation - not grunk.dll/grunk_recipe.dll/grunk_plugin.dll,
# freshly built into the tree, never installed into the conda env). Copying every DLL
# runUnitTests actually depends on (transitively - this also covers plugin_native_
# fixture*'s own dependency on grunk's DLLs below, since those land in this same
# directory by CMake's own default per-CMakeLists.txt output layout) next to the exe
# itself is the standard, documented fix for this exact scenario.
if (WIN32)
    add_custom_command(TARGET runUnitTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:runUnitTests>
            $<TARGET_FILE_DIR:runUnitTests>
        COMMAND_EXPAND_LISTS
    )
endif()

if (GRUNK_WITH_PLUGIN)
    target_compile_definitions(runUnitTests PRIVATE
        NATIVE_FIXTURE_SO_PATH="$<TARGET_FILE:plugin_native_fixture>"
        FAILING_NATIVE_FIXTURE_SO_PATH="$<TARGET_FILE:plugin_native_fixture_failing>"
        NONSTD_THROW_FIXTURE_SO_PATH="$<TARGET_FILE:plugin_native_fixture_nonstd_throw>"
    )
    add_dependencies(runUnitTests plugin_native_fixture plugin_native_fixture_failing plugin_native_fixture_nonstd_throw)
endif()

