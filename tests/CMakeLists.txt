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
  RuntimeAlgorithm.cpp
  Feature.cpp
  RuntimeFeature.cpp
)

add_executable(runUnitTests ${TESTSOURCES})
target_link_libraries(runUnitTests PRIVATE grunk gtest_main)
