# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

if(MSVC)
    #https://developercommunity.visualstudio.com/t/error-C2187-syntax-error:-attribute-sp/10439022
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:lambda")
endif()

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/version.hpp.in
        ${CMAKE_CURRENT_BINARY_DIR}/version.hpp
)

# grunk/recipe.hpp #include "grunk/version.hpp"s this directly - never installed
# before now because nothing ever consumed grunk via find_package() (this file
# only ever needed to exist in the *build* tree, reachable via each module's own
# $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..> include entry) until
# grunk-occt/grunk-adolc switched from vendoring grunk's source to find_package.
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/version.hpp DESTINATION include/grunk)

if (NOT EXISTS "${CMAKE_BINARY_DIR}/.gitignore")
    file(WRITE "${CMAKE_BINARY_DIR}/.gitignore" "*\n")
endif()

# Add the library grunk. SHARED specifically when it has a real translation unit
# (GRUNK_WITH_RECIPE) - see ../CMakeLists.txt's own RPATH comment for why this
# matters (one dynamically-shared copy of grunk's dispatch state per process, not one
# statically-embedded copy per consumer). The GRUNK_WITH_RECIPE=OFF branch keeps its
# original plain add_library(grunk) (STATIC, empty) unchanged - it has zero sources of
# its own (grunk::core/grunk::dynamic are both INTERFACE/header-only), and further
# down this file still does `target_link_libraries(grunk PUBLIC ...)` under
# GRUNK_WITH_PLUGIN, which is only valid for a real library target, not an INTERFACE
# one - no downstream repo in this project ever builds with GRUNK_WITH_RECIPE=OFF, so
# this combination isn't part of what this change needs to cover.
if (GRUNK_WITH_RECIPE)
        # ensure the library has at least one source so CMake won't complain
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/grunk_version.cpp"
"/* auto-generated */\n#include \"version.hpp\"\n")
        add_library(grunk SHARED "${CMAKE_CURRENT_BINARY_DIR}/grunk_version.cpp")
else()
        add_library(grunk INTERFACE)
endif()

add_subdirectory(core)
target_link_libraries(grunk INTERFACE grunk::core)

if (GRUNK_WITH_DYNAMIC)
        add_subdirectory(dynamic)
        target_link_libraries(grunk INTERFACE grunk::dynamic)
endif()

if (GRUNK_WITH_RECIPE)
        add_subdirectory(recipe)
        target_link_libraries(grunk PUBLIC grunk::recipe)
endif()

if (GRUNK_WITH_PLUGIN)
        add_subdirectory(plugin)
        target_link_libraries(grunk PUBLIC grunk::plugin)
endif()

# install grunk
install(TARGETS grunk
        EXPORT grunkTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

set_target_properties(grunk PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        EXPORT_NAME grunk
)

# install core.hpp and grunk.hpp
install(FILES
    ${PROJECT_SOURCE_DIR}/grunk/core.hpp ${PROJECT_SOURCE_DIR}/grunk/grunk.hpp
    DESTINATION include/grunk
)

if (GRUNK_WITH_DYNAMIC)
    # install dynamic.hpp
    install(FILES
        ${PROJECT_SOURCE_DIR}/grunk/dynamic.hpp
        DESTINATION include/grunk
    )
endif()

if (GRUNK_WITH_RECIPE)
    # install recipe.hpp
    install(FILES
        ${PROJECT_SOURCE_DIR}/grunk/recipe.hpp
        DESTINATION include/grunk
    )
endif()

if (GRUNK_WITH_PLUGIN)
    # install plugin.hpp
    install(FILES
        ${PROJECT_SOURCE_DIR}/grunk/plugin.hpp
        DESTINATION include/grunk
    )
endif()