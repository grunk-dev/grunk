cmake_minimum_required(VERSION 3.13)
project(grunk)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

option(GRUNK_TESTS "Build unit tests using GTest" OFF)

# conan creates FindPackage.cmake files in the CMAKE_BINARY_DIR
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})

# grunk needs to be a shared library
set(BUILD_SHARED_LIBS  ON)

# find dependencies
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
    grunk/plugins/IPlugin.h
    grunk/plugins/PluginRegistry.h
    grunk/plugins/PluginRegistry.cpp
    grunk/plugins/StdPlugin.h 
    grunk/plugins/StdPlugin.cpp
    grunk/runtime/RuntimeAlgorithm.h
    grunk/runtime/RuntimeAlgorithm.cpp
    grunk/runtime/FunctionRegistry.h
    grunk/runtime/FunctionRegistry.cpp
    grunk/runtime/RuntimeFeature.h
    grunk/runtime/RuntimeFeature.cpp
    grunk/runtime/RuntimeObject.h
    grunk/runtime/RuntimeObject.cpp
    grunk/runtime/RuntimeFunction.h
    grunk/runtime/RuntimeFunction.cpp
    grunk/grunk.h
)

# create a library and add source files
add_library(grunk ${grunk_SRC})

target_compile_features(grunk PUBLIC cxx_std_17)
target_link_libraries(grunk PUBLIC parametric::parametric 
                                   Boost::headers
                                   reflect::reflect
                                   ${CMAKE_DL_LIBS})
target_include_directories(grunk PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include/grunk> 
)

include(GNUInstallDirs)

add_subdirectory(docs)

if (GRUNK_TESTS)
        add_subdirectory(tests)
endif()

# install binaries
install(TARGETS grunk
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/grunk
        DESTINATION include/grunk
        FILES_MATCHING
        REGEX ".*\\.(h|hpp|hxx|inl)"
)
