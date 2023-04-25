if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
endif()

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
message(STATUS "Python3 interpreter:" ${Python3_EXECUTABLE})
message(STATUS "Python include directory: ${Python3_INCLUDE_DIR}")
message(STATUS "Python library release: ${Python3_LIBRARY_RELEASE}")

find_package(SWIG REQUIRED)

include(${SWIG_USE_FILE})
include_directories(${PYTHON_INCLUDE_PATH})
set (MODULES "grunk")
set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})
set(SWIG_FLAGS "-Wall -c++ -python -std=c++17")

include_directories($<TARGET_PROPERTY:grunk,INTERFACE_INCLUDE_DIRECTORIES>)

foreach(MODULE ${MODULES})

    set_source_files_properties(${MODULE}.i PROPERTIES CPLUSPLUS ON)

    swig_add_library(${MODULE} LANGUAGE python SOURCES ${MODULE}.i TYPE MODULE)
    swig_link_libraries(${MODULE} grunk Python3::Module)
    set_property(TARGET ${MODULE} PROPERTY COMPILE_OPTIONS -shared -std=c++17 ) 

    install(TARGETS _${MODULE}
            DESTINATION share/grunk/python/grunk
            COMPONENT interfaces
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${MODULE}.py
            DESTINATION share/grunk/python/grunk
            COMPONENT interfaces
    )

endforeach(MODULE)