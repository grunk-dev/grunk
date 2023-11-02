cmake_minimum_required(VERSION 3.13)

set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})

find_package(grunk REQUIRED)

add_executable(load_carrier load_carrier.cpp)

target_link_libraries(load_carrier grunk::grunk)

target_compile_features(load_carrier PRIVATE cxx_std_17)