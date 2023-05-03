cmake_minimum_required(VERSION 3.4)
project(grunk-tests)

if (NOT TARGET "grunk")
  find_package(grunk REQUIRED)
endif()

enable_testing()
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(GOOGLE_TEST_INDIVIDUAL ON)
include(AddGoogleTest)

set(TESTSOURCES
  testmain.cpp
  Action.cpp
  io.cpp
  DynamicAction.cpp
  Feature.cpp
  DynamicFeature.cpp
)

add_library(SimplePlugin SHARED plugin_simple.cpp)
target_link_libraries(SimplePlugin PRIVATE grunk)

add_executable(runUnitTests ${TESTSOURCES})
target_link_libraries(runUnitTests PRIVATE grunk gtest_main)
add_dependencies(runUnitTests SimplePlugin)

add_custom_command(
  TARGET runUnitTests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/../test_data
          ${CMAKE_CURRENT_BINARY_DIR}/test_data
)
