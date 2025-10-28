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

if (GRUNK_WITH_RECIPE)
        # ensure the library has at least one source so CMake won't complain
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/grunk_version.cpp"
"/* auto-generated */\n#include \"version.hpp\"\n")
        add_library(grunk "${CMAKE_CURRENT_BINARY_DIR}/grunk_version.cpp")
else()
        add_library(grunk)
endif()

add_subdirectory(dynamic)
target_link_libraries(grunk INTERFACE grunk::dynamic)

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

# install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
        DESTINATION include
        FILES_MATCHING
        REGEX ".*\\.(h|hpp|hxx|inl)$"
        PATTERN "CMakeFiles" EXCLUDE
        PATTERN ".git" EXCLUDE
)