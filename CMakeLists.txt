cmake_minimum_required(VERSION 3.12)

project(grunk VERSION 0.5.0)

include(GNUInstallDirs)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

option(GRUNK_WITH_RECIPE "Builds the grunk::recipe library, for exporting and importing recipes from/to yaml format" ON)
option(GRUNK_PYTHON      "Build python bindings using pybind11" OFF)

option(GRUNK_TESTS "Build unit tests using GTest" OFF)


# conan creates FindPackage.cmake files in the CMAKE_BINARY_DIR
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})

# find dependencies
find_package(parametric REQUIRED)
find_package(sol2 REQUIRED)
find_package(Lua REQUIRED)

# Windows specific: export all symbols
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_subdirectory(grunk)

if (GRUNK_TESTS)
        add_subdirectory(tests/cpp)
endif()

# Create a package configuration file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/grunkConfigVersion.cmake"
        VERSION grunk_VERSION
        COMPATIBILITY AnyNewerVersion
)

install(EXPORT grunkTargets
        FILE grunkTargets.cmake
        NAMESPACE grunk::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/grunk
)

configure_package_config_file(
        "${CMAKE_SOURCE_DIR}/cmake/grunkConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/grunkConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/grunk
)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/grunkConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/grunkConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/grunk
)
