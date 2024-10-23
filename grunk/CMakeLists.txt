set(grunk_SRC
    internal/ActionDynamic.hpp      internal/ActionDynamic.cpp
    internal/ActionStatic.hpp       internal/ActionStatic.cpp
    internal/ResultHolder.hpp       internal/ResultHolder.cpp
    action.hpp                      action.cpp
    feature.hpp                     feature.cpp
    object.hpp                      object.cpp
    state.hpp                       state.cpp 
)

if(MSVC)
    #https://developercommunity.visualstudio.com/t/error-C2187-syntax-error:-attribute-sp/10439022
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:lambda")
endif()

# create a library and add source files
add_library(grunk ${grunk_SRC})

target_compile_features(grunk PUBLIC cxx_std_17)
target_link_libraries(grunk PUBLIC ${CMAKE_DL_LIBS}
                                   parametric::parametric 
                                   sol2::sol2
				   # this is needed to support older gcc versions:
				   $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:stdc++fs>)
target_include_directories(grunk PUBLIC
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
