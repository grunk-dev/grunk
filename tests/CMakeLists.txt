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
  Algorithm.cpp
  FunctionRegistry.cpp
  Feature.cpp
  RuntimeFunction.cpp
  RuntimeObject.cpp
)

add_executable(runUnitTests ${TESTSOURCES})
target_link_libraries(runUnitTests PRIVATE grunk gtest_main)
#add_gtest(runUnitTests)

#include(CTest)
#add_test(NAME test COMMAND runUnitTests WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
