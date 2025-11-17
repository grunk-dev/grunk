cmake_minimum_required(VERSION 3.13)

set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})

find_package(grunk REQUIRED)

add_executable(example example.cpp)

target_link_libraries(example grunk::grunk)

target_compile_features(example PRIVATE cxx_std_17)
