project(PackageTest CXX)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_library(bar SHARED plugin.cpp)
target_link_libraries(bar ${CONAN_LIBS})
target_compile_features(bar PUBLIC cxx_std_17)

add_executable(example example.cpp)
target_link_libraries(example ${CONAN_LIBS})
target_compile_features(example PUBLIC cxx_std_17)