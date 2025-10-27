include(FetchContent)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0
)
FetchContent_MakeAvailable(yaml-cpp)

# create a library and add source files
set(grunk_recipe_src
    Recipe.hpp Recipe.cpp
)

add_library(grunk_recipe ${grunk_recipe_src})

target_link_libraries(grunk_recipe PUBLIC grunk::dynamic yaml-cpp::yaml-cpp)
target_include_directories(grunk_recipe INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include/grunk> 
)

add_library(grunk::recipe ALIAS grunk_recipe)