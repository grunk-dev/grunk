# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

# create a library and add source files
add_library(grunk_dynamic INTERFACE)
add_library(grunk::dynamic ALIAS grunk_dynamic)

# see ../core/CMakeLists.txt's own comment on why this is needed for find_package(grunk)
set_target_properties(grunk_dynamic PROPERTIES EXPORT_NAME dynamic)

if (GRUNK_WITH_RECIPE)
  target_compile_definitions(grunk_dynamic INTERFACE GRUNK_WITH_RECIPE)
endif()

# sol2 defaults to UNCHECKED argument/userdata extraction (SOL_ALL_SAFETIES_ON
# off unless this is a debug build) - a Lua value of one registered usertype
# passed where a *different* usertype is expected (e.g. a plugin's own
# implicitly-convertible wrapper type passed where the type it converts to is
# expected) is blindly reinterpreted as the target type instead of being
# rejected, corrupting memory and segfaulting deep inside unrelated code
# instead of failing at the call site with a clear error - found the hard way
# via grunk-geoml's own airplane_seats.grr.yml (geoml::Shape passed directly
# where a TopoDS_Shape was expected). Propagated as an INTERFACE compile
# definition (not just set for grunk's own sources) so every downstream
# consumer that links against grunk::dynamic - directly or transitively, e.g.
# a plugin's own generated registration code - gets the same, consistent
# define: sol2 template instantiations must agree on this across every
# translation unit that touches the same types, so this cannot be a
# per-project opt-in without risking ODR/ABI mismatches.
target_compile_definitions(grunk_dynamic INTERFACE SOL_ALL_SAFETIES_ON=1)

target_compile_features(grunk_dynamic INTERFACE cxx_std_17)
target_link_libraries(grunk_dynamic INTERFACE grunk::core sol2::sol2 ${LUA_LIBRARIES})

target_include_directories(grunk_dynamic INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
  # see ../core/CMakeLists.txt's own comment on why this second entry is needed
  $<INSTALL_INTERFACE:include/grunk/dynamic>
)

install(TARGETS grunk_dynamic
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
