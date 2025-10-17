# create a library and add source files
add_library(grunk_core INTERFACE)
add_library(grunk::core ALIAS grunk_core)


if (GRUNK_BUILD_RECIPE_LIB)
    target_compile_definitions(grunk_core INTERFACE _BUILD_RECIPE_LIB)
endif()

target_compile_features(grunk_core INTERFACE cxx_std_17)
target_link_libraries(grunk_core INTERFACE parametric sol2::sol2 ${LUA_LIBRARIES})
target_include_directories(grunk_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include> 
)
