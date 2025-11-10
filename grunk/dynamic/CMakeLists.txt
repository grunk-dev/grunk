# create a library and add source files
add_library(grunk_dynamic INTERFACE)
add_library(grunk::dynamic ALIAS grunk_dynamic)

if (GRUNK_WITH_RECIPE)
  target_compile_definitions(grunk_dynamic INTERFACE GRUNK_WITH_RECIPE)
endif()

target_compile_features(grunk_dynamic INTERFACE cxx_std_17)

target_link_libraries(grunk_dynamic INTERFACE grunk::core sol2::sol2 ${LUA_LIBRARIES})
if (GRUNK_WITH_TASKFLOW)
    target_link_libraries(grunk_dynamic INTERFACE Taskflow::Taskflow)
    target_compile_definitions(grunk_dynamic INTERFACE GRUNK_WITH_TASKFLOW)
endif()

target_include_directories(grunk_dynamic INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include/grunk> 
)

install(TARGETS grunk_dynamic
  EXPORT grunkTargets
)

# install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
        DESTINATION include
        FILES_MATCHING
        REGEX ".*\\.(h|hpp|hxx|inl)$"
        PATTERN "CMakeFiles" EXCLUDE
        PATTERN ".git" EXCLUDE
)
