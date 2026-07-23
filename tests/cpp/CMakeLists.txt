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
    )
endif()

if (GRUNK_WITH_RECIPE)
    list(APPEND TESTSOURCES
      recipe_simple.cpp
      recipe_clone.cpp
      recipe_call.cpp
      recipe_module.cpp
    )
endif()

add_executable(runUnitTests ${TESTSOURCES})
target_link_libraries(runUnitTests PRIVATE grunk gtest_main)

