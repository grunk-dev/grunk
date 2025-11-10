# create a library and add source files
add_library(grunk_core INTERFACE)
add_library(grunk::core ALIAS grunk_core)

if (GRUNK_WITH_DYNAMIC)
  target_compile_definitions(grunk_core INTERFACE GRUNK_WITH_DYNAMIC)
endif()

target_compile_features(grunk_core INTERFACE cxx_std_17)

target_link_libraries(grunk_core INTERFACE parametric)
if (GRUNK_WITH_TASKFLOW)
    target_link_libraries(grunk_core INTERFACE Taskflow::Taskflow)
    target_compile_definitions(grunk_core INTERFACE GRUNK_WITH_TASKFLOW)
endif()

target_include_directories(grunk_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include/grunk> 
)

install(TARGETS grunk_core
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
