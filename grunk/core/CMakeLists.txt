if(MSVC)
    #https://developercommunity.visualstudio.com/t/error-C2187-syntax-error:-attribute-sp/10439022
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:lambda")
endif()

# create a library and add source files
add_library(grunk_core INTERFACE)
add_library(grunk::core ALIAS grunk_core)

target_compile_features(grunk_core INTERFACE cxx_std_17)
target_link_libraries(grunk_core INTERFACE parametric sol2::sol2 ${LUA_LIBRARIES})
target_include_directories(grunk_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/..
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/..
  $<INSTALL_INTERFACE:include/grunk> 
)

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
