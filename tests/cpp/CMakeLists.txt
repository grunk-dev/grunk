cmake_minimum_required(VERSION 3.4)
project(grunk-tests)

if (NOT TARGET grunk)
  find_package(grunk REQUIRED COMPONENTS dynamic recipe)
endif()

enable_testing()
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(GOOGLE_TEST_INDIVIDUAL ON)
include(AddGoogleTest)

set(TESTSOURCES
  testmain.cpp
  dynamic_operators.cpp
  dynamic_serialization.cpp
  dynamic_state.cpp
  dynamic_std_vector.cpp
  recipe_simple.cpp
  recipe_clone.cpp
)


add_executable(runUnitTests ${TESTSOURCES})
target_link_libraries(runUnitTests PRIVATE grunk gtest_main)

# add_custom_command(
#   TARGET runUnitTests POST_BUILD
#   COMMAND ${CMAKE_COMMAND} -E copy_directory
#           ${CMAKE_CURRENT_SOURCE_DIR}/../test_data
#           ${CMAKE_CURRENT_BINARY_DIR}/test_data
# )

