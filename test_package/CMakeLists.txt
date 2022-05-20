project(PackageTest CXX)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()
message("Conan libs = ${CONAN_LIBS}")

add_library(bar SHARED plugin.cpp)
target_link_libraries(bar ${CONAN_LIBS})
target_compile_features(bar PUBLIC cxx_std_17)

add_executable(example example.cpp)
target_link_libraries(example ${CONAN_LIBS})
target_compile_features(example PUBLIC cxx_std_17)