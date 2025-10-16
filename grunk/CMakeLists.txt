if(MSVC)
    #https://developercommunity.visualstudio.com/t/error-C2187-syntax-error:-attribute-sp/10439022
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:lambda")
endif()

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/version.hpp.in 
        ${CMAKE_CURRENT_BINARY_DIR}/version.hpp
)

add_library(grunk INTERFACE)

add_subdirectory(core)
target_link_libraries(grunk INTERFACE grunk::core)

option(GRUNK_BUILD_RECIPE_LIB "Builds the grunk::recipe library, for exporting and importing recipes from/to yaml format" ON)
if (GRUNK_BUILD_RECIPE_LIB)
    add_subdirectory(recipe)
    target_link_libraries(grunk INTERFACE grunk::recipe)
endif()

# install grunk
install(TARGETS grunk_core
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
        DESTINATION include/grunk
        FILES_MATCHING
        REGEX ".*\\.(h|hpp|hxx|inl)$"
)