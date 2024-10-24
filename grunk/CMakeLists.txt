if(MSVC)
    #https://developercommunity.visualstudio.com/t/error-C2187-syntax-error:-attribute-sp/10439022
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:lambda")
endif()

# create a library and add source files
add_library(grunk INTERFACE)

target_compile_features(grunk INTERFACE cxx_std_17)
target_link_libraries(grunk INTERFACE parametric::parametric
                                   sol2::sol2)
target_include_directories(grunk INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<INSTALL_INTERFACE:include/grunk> 
)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/internal/version.hpp.in 
        ${CMAKE_CURRENT_BINARY_DIR}/internal/version.hpp
)

# install grunk
install(TARGETS grunk
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
