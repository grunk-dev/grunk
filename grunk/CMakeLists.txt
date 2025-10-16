configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/version.hpp.in 
        ${CMAKE_CURRENT_BINARY_DIR}/version.hpp
)

add_subdirectory(core)

add_library(grunk INTERFACE)
target_link_libraries(grunk INTERFACE grunk::core)

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