set(grunk_SRC
    compute_nodes/Action.hpp
    compute_nodes/Action.cpp
    compute_nodes/DynamicAction.hpp
    compute_nodes/DynamicAction.cpp
    compute_nodes/DynamicVec.hpp
    compute_nodes/DynamicVec.cpp
    compute_nodes/Expression.hpp
    compute_nodes/Expression.cpp
    compute_nodes/Script.hpp
    compute_nodes/Script.cpp
    compute_nodes/Vec.hpp
    compute_nodes/Vec.cpp
    common/common_functions.hpp
    common/common_functions.cpp
    common/init.hpp
    common/init.cpp
    common/io_error.hpp
    common/io.hpp
    common/io.cpp
    common/parametric_core.hpp
    common/ResultHolder.hpp
    common/ResultHolder.cpp
    common/String.hpp
    common/String.cpp
    plugins/IPlugin.hpp
    plugins/PluginRegistry.hpp
    plugins/PluginRegistry.cpp
    DynamicFeature.hpp
    DynamicFeature.inl
    DynamicFeature.cpp
    Feature.hpp
    Feature.cpp
    FeatureBase.hpp
    FeatureBase.cpp
    Recipe.hpp
    Recipe.cpp
    action.hpp
    action.cpp
    expression.hpp
    expression.cpp
    grunk.hpp
    script.hpp
    script.cpp
    vec.hpp
    vec.cpp
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
                                   reflect::reflect
                                   Boost::headers
                                   yaml-cpp
                                   muparser::muparser
				   # this is needed to support older gcc versions:
				   $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:stdc++fs>)
target_include_directories(grunk PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
  $<INSTALL_INTERFACE:include/grunk> 
)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/common/version.hpp.in 
        ${CMAKE_CURRENT_BINARY_DIR}/common/version.hpp
)

# install grunk
install(TARGETS grunk
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
        DESTINATION include
        FILES_MATCHING
        REGEX ".*\\.(h|hpp|hxx|inl)$"
)
