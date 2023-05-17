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