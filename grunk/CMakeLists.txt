cmake_minimum_required(VERSION 3.13)
project(grunk)


# conan creates FindPackage.cmake files in the CMAKE_BINARY_DIR
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})

# find dependencies
find_package(Boost REQUIRED COMPONENTS headers)
find_package(range-v3 REQUIRED)
find_package(parametric REQUIRED)
find_package(reflect REQUIRED)

# Windows specific: export all symbols
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# create a library and add source files
add_library(grunk plugins/api.h
                  plugins/PluginRegistry.h
                  plugins/PluginRegistry.cpp
                  plugins/StdPlugin.h 
                  plugins/StdPlugin.cpp
                  Algorithm.h
                  Algorithm.cpp
                  RuntimeObject.h
                  RuntimeObject.cpp
)

target_compile_features(grunk PUBLIC cxx_std_17)
target_link_libraries(grunk PRIVATE parametric::parametric 
                                    Boost::headers
                                    range-v3::range-v3
                                    reflect::reflect
                                    ${CMAKE_DL_LIBS})
target_include_directories(grunk PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include/grunk> 
)

# install binaries
install(TARGETS grunk
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/grunk
        FILES_MATCHING
        REGEX ".*\\.(h|hpp|hxx|inl)"
)
