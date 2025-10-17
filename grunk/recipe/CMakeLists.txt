# create a library and add source files
set(grunk_recipe_src
    io.hpp
    io.cpp
    io_error.hpp
    io_error.cpp
    parametric_core.hpp
)

add_library(grunk_recipe ${grunk_recipe_src})

target_link_libraries(grunk_recipe PUBLIC grunk::core)
target_include_directories(grunk_recipe INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include> 
)

add_library(grunk::recipe ALIAS grunk_recipe)