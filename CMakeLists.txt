cmake_minimum_required(VERSION 3.12)

project(grunk VERSION 0.3.1)

include(GNUInstallDirs)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

option(GRUNK_TESTS "Build unit tests using GTest" OFF)
option(GRUNK_PYTHON "Build python bindings using pybind11" OFF)


# conan creates FindPackage.cmake files in the CMAKE_BINARY_DIR
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})

# grunk needs to be a shared library
set(BUILD_SHARED_LIBS  ON)

# find dependencies
find_package(yaml-cpp REQUIRED)
find_package(muparser REQUIRED)
find_package(Boost REQUIRED COMPONENTS headers)
find_package(parametric REQUIRED)
find_package(reflect REQUIRED)

# Windows specific: export all symbols
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_subdirectory(grunk)

add_subdirectory(docs)

if (GRUNK_PYTHON)
        add_subdirectory(python)
endif()


if (GRUNK_TESTS)
        add_subdirectory(tests/cpp)
endif()
