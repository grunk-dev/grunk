# create a library and add source files
add_library(grunk_dynamic INTERFACE)
add_library(grunk::dynamic ALIAS grunk_dynamic)

target_compile_features(grunk_dynamic INTERFACE cxx_std_17)
target_link_libraries(grunk_dynamic INTERFACE parametric sol2::sol2 ${LUA_LIBRARIES})
target_include_directories(grunk_dynamic INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include/grunk> 
)
