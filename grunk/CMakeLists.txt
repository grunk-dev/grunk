if(MSVC)
    #https://developercommunity.visualstudio.com/t/error-C2187-syntax-error:-attribute-sp/10439022
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:lambda")
endif()

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/version.hpp.in 
        ${CMAKE_CURRENT_BINARY_DIR}/version.hpp
)

if (NOT EXISTS "${CMAKE_BINARY_DIR}/.gitignore")
    file(WRITE "${CMAKE_BINARY_DIR}/.gitignore" "*\n")
endif()

# Add the library grunk
if (GRUNK_WITH_RECIPE)
        # ensure the library has at least one source so CMake won't complain
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/grunk_version.cpp"
"/* auto-generated */\n#include \"version.hpp\"\n")
        add_library(grunk "${CMAKE_CURRENT_BINARY_DIR}/grunk_version.cpp")
else()
        add_library(grunk)
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