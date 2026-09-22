# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

# create a library and add source files
add_library(grunk_core INTERFACE)
add_library(grunk::core ALIAS grunk_core)

# ALIAS targets (above) are local to this build - install(EXPORT ...) below exports
# the *real* target under its own name (grunk_core) unless EXPORT_NAME says
# otherwise. Set explicitly so a consumer using find_package(grunk) gets the same
# grunk::core name every in-tree target_link_libraries(... grunk::core ...) call
# already uses, rather than the redundant grunk::grunk_core - confirmed to matter,
# not just cosmetic: grunk-occt/grunk-adolc's own CMakeLists.txt link grunk::plugin/
# grunk::recipe by name, and would otherwise fail to find them post-find_package.
set_target_properties(grunk_core PROPERTIES EXPORT_NAME core)

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
  $<INSTALL_INTERFACE:include>
  # matches the BUILD_INTERFACE's own CMAKE_CURRENT_SOURCE_DIR entry above - some
  # headers in this module #include a sibling by bare filename (e.g. <object.hpp>),
  # not the full <grunk/core/object.hpp> path, so the installed layout needs this
  # module's own installed subdirectory on the include path too, not just the
  # top-level include/ that covers the <grunk/...>-style includes.
  $<INSTALL_INTERFACE:include/grunk/core>
)

install(TARGETS grunk_core
  EXPORT grunkTargets
)

# install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
        DESTINATION include/grunk
        FILES_MATCHING
        REGEX ".*\\.(h|hpp|hxx|inl)$"
        PATTERN "CMakeFiles" EXCLUDE
        PATTERN ".git" EXCLUDE
)
