# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

# create a library and add source files
set(grunk_recipe_src
    Recipe.hpp Recipe.cpp
    RecipeAction.hpp RecipeAction.cpp
    RecipeGetValAction.hpp RecipeGetValAction.cpp
    RecipeCaller.hpp RecipeCaller.cpp
    ModuleAction.hpp ModuleAction.cpp
)

# SHARED - see ../../CMakeLists.txt's RPATH comment: one dynamically-linked copy of
# grunk's dispatch state per process, not a statically-embedded copy per consumer.
add_library(grunk_recipe SHARED ${grunk_recipe_src})

target_link_libraries(grunk_recipe PUBLIC grunk::dynamic PRIVATE yaml-cpp::yaml-cpp)
target_include_directories(grunk_recipe PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
  # see ../core/CMakeLists.txt's own comment on why this second entry is needed
  $<INSTALL_INTERFACE:include/grunk/recipe>
)

add_library(grunk::recipe ALIAS grunk_recipe)

# see ../core/CMakeLists.txt's own comment on why this is needed for find_package(grunk)
set_target_properties(grunk_recipe PROPERTIES EXPORT_NAME recipe)

install(TARGETS grunk_recipe
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