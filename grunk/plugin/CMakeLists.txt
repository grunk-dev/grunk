# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

add_library(grunk_plugin loader.cpp)
add_library(grunk::plugin ALIAS grunk_plugin)

target_compile_features(grunk_plugin PUBLIC cxx_std_17)
target_link_libraries(grunk_plugin PUBLIC grunk::dynamic PRIVATE ${CMAKE_DL_LIBS})

# Lets grunk/grunk.hpp's own `#ifdef GRUNK_WITH_PLUGIN` (mirroring GRUNK_WITH_DYNAMIC/
# GRUNK_WITH_RECIPE) know to pull in plugin.hpp, for anything linking grunk::plugin
# (directly or transitively via the grunk umbrella target).
target_compile_definitions(grunk_plugin PUBLIC GRUNK_WITH_PLUGIN)

target_include_directories(grunk_plugin PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include/grunk>
)

install(TARGETS grunk_plugin
        EXPORT grunkTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
        DESTINATION include/grunk
        FILES_MATCHING
        REGEX ".*\\.(h|hpp|hxx|inl)$"
        PATTERN "CMakeFiles" EXCLUDE
        PATTERN ".git" EXCLUDE
)
