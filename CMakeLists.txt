cmake_minimum_required(VERSION 3.12)

project(grunk VERSION 0.1.0)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

option(GRUNK_TESTS "Build unit tests using GTest" OFF)

# conan creates FindPackage.cmake files in the CMAKE_BINARY_DIR
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})

# grunk needs to be a shared library
set(BUILD_SHARED_LIBS  ON)

# find dependencies
find_package(yaml-cpp REQUIRED)
find_package(Boost REQUIRED COMPONENTS headers)
find_package(parametric REQUIRED)
find_package(reflect REQUIRED)

# Windows specific: export all symbols
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(grunk_SRC
    grunk/core/Action.hpp
    grunk/core/Action.cpp
    grunk/core/Feature.hpp
    grunk/core/Feature.cpp
    grunk/io/io.hpp 
    grunk/io/io.cpp
    grunk/plugins/IPlugin.hpp
    grunk/plugins/PluginRegistry.hpp
    grunk/plugins/PluginRegistry.cpp
    grunk/plugins/StdPlugin.hpp
    grunk/plugins/StdPlugin.cpp
    grunk/dynamic/RuntimeAction.hpp
    grunk/dynamic/RuntimeAction.cpp
    grunk/dynamic/RuntimeFeature.hpp
    grunk/dynamic/RuntimeFeature.cpp
    grunk/grunk.hpp
)

# create a library and add source files
add_library(grunk ${grunk_SRC})

target_compile_features(grunk PUBLIC cxx_std_17)
target_link_libraries(grunk PUBLIC ${CMAKE_DL_LIBS}
                                   parametric::parametric 
                                   reflect::reflect
                                   Boost::headers
                                   yaml-cpp::yaml-cpp
				   # this is needed to support older gcc versions:
				   $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:stdc++fs>)
target_include_directories(grunk PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<INSTALL_INTERFACE:include/grunk> 
)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/grunk/version.hpp.in 
        ${CMAKE_CURRENT_BINARY_DIR}/grunk/version.hpp
)

include(GNUInstallDirs)

add_subdirectory(docs)

if (GRUNK_TESTS)
        add_subdirectory(tests)
endif()

# install grunk
install(TARGETS grunk
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/grunk ${CMAKE_CURRENT_BINARY_DIR}/grunk
        DESTINATION include
        FILES_MATCHING
        REGEX ".*\\.(h|hpp|hxx|inl)$"
)
