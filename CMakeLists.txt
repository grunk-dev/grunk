cmake_minimum_required(VERSION 3.13)

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
    grunk/core/Algorithm.h
    grunk/core/Algorithm.cpp
    grunk/core/Feature.h
    grunk/core/Feature.cpp
    grunk/io/io.h 
    grunk/io/io.cpp
    grunk/plugins/IPlugin.h
    grunk/plugins/PluginRegistry.h
    grunk/plugins/PluginRegistry.cpp
    grunk/plugins/StdPlugin.h 
    grunk/plugins/StdPlugin.cpp
    grunk/dynamic/RuntimeAlgorithm.h
    grunk/dynamic/RuntimeAlgorithm.cpp
    grunk/dynamic/RuntimeFeature.h
    grunk/dynamic/RuntimeFeature.cpp
    grunk/grunk.h
)

# create a library and add source files
add_library(grunk ${grunk_SRC})

target_compile_features(grunk PUBLIC cxx_std_17)
target_link_libraries(grunk PUBLIC yaml-cpp::yaml-cpp
                                   parametric::parametric 
                                   Boost::headers
                                   reflect::reflect
                                   ${CMAKE_DL_LIBS})
target_include_directories(grunk PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<INSTALL_INTERFACE:include/grunk> 
)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/grunk/version.h.in 
        ${CMAKE_CURRENT_BINARY_DIR}/grunk/version.h
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
