project(PackageTest CXX)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_library(pluginA SHARED pluginA.hpp pluginA.cpp)
target_link_libraries(pluginA ${CONAN_LIBS})
target_compile_features(pluginA PUBLIC cxx_std_17)
target_include_directories(pluginA PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

add_library(pluginB SHARED pluginB.cpp)
target_link_libraries(pluginB ${CONAN_LIBS} pluginA)
target_compile_features(pluginB PUBLIC cxx_std_17)

add_executable(example example.cpp)
target_link_libraries(example ${CONAN_LIBS} ${CMAKE_DL_LIBS})
target_compile_features(example PUBLIC cxx_std_17)