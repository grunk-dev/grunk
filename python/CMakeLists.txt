option(GRUNK_USE_BUNDLED_PYBIND11 "Use pybind11 bundled with grunk" ON)
mark_as_advanced(GRUNK_USE_BUNDLED_PYBIND11)

if(GRUNK_USE_BUNDLED_PYBIND11)
    # Fetch pybind11
    include(FetchContent)
    FetchContent_Declare(pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11
        GIT_TAG v2.10.4
    )
    FetchContent_GetProperties(pybind11)

    if(NOT pybind11_POPULATED)
        FetchContent_Populate(pybind11)
        add_subdirectory(${pybind11_SOURCE_DIR} ${pybind11_BINARY_DIR})
    endif()
else()
    find_package(pybind11 REQUIRED)
endif()

if(MSVC)
    #https://developercommunity.visualstudio.com/t/error-C2187-syntax-error:-attribute-sp/10439022
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:lambda")
endif()

pybind11_add_module(_core MODULE
    _core.cpp
)
target_link_libraries(_core PRIVATE grunk)

# copy grunk.dll (.so) to site-package grunk folder
add_custom_command(TARGET _core POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:grunk>
        $<TARGET_FILE_DIR:_core>
)

set(reflect_dll ${reflect_LIBRARIES})
if(MSVC)
    # This is a bit hacky. Locate the reflect.dll and copy it 
    # to the site-packages folder manually
    find_file(
        reflect_dll 
        NAMES reflect.dll 
        PATHS ${reflect_INCLUDE_DIRS}/../bin
    )
else()
    find_file(
        reflect_dll 
        NAMES libreflect.so
        PATHS ${reflect_INCLUDE_DIRS}/../bin
    )
endif()
    
add_custom_command(TARGET _core POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${reflect_dll}
        $<TARGET_FILE_DIR:_core>
)
