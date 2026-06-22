# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.15)
project(cad_autodiff_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_PREFIX_PATH "$ENV{CONDA_PREFIX}")

file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/cmake")

include(FetchContent)

FetchContent_Declare(geoml
    GIT_REPOSITORY https://github.com/DLR-SC/geoml.git
    GIT_TAG main
)

FetchContent_GetProperties(geoml)
if(NOT geoml_POPULATED)
    cmake_policy(SET CMP0169 OLD)
    FetchContent_Populate(geoml)

    if(EXISTS "${geoml_SOURCE_DIR}/cmake/geomlConfig.cmake.in")
        file(COPY "${geoml_SOURCE_DIR}/cmake/geomlConfig.cmake.in"
             DESTINATION "${CMAKE_SOURCE_DIR}/cmake"
             NO_SOURCE_PERMISSIONS)
    endif()

    add_subdirectory(${geoml_SOURCE_DIR} ${geoml_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET grunk)
    FetchContent_Declare(grunk
        GIT_REPOSITORY https://github.com/grunk-dev/grunk.git
        GIT_TAG main
    )

    FetchContent_GetProperties(grunk)
    if(NOT grunk_POPULATED)
        cmake_policy(SET CMP0169 OLD)
        FetchContent_Populate(grunk)

        if(EXISTS "${grunk_SOURCE_DIR}/cmake/grunkConfig.cmake.in")
            file(COPY "${grunk_SOURCE_DIR}/cmake/grunkConfig.cmake.in"
                 DESTINATION "${CMAKE_SOURCE_DIR}/cmake"
                 NO_SOURCE_PERMISSIONS)
        endif()

        set(GRUNK_WITH_TASKFLOW OFF CACHE BOOL "" FORCE)
        set(GRUNK_WITH_DYNAMIC ON CACHE BOOL "" FORCE)
        set(GRUNK_WITH_RECIPE ON CACHE BOOL "" FORCE)

        include_directories(${grunk_BINARY_DIR})
        add_subdirectory(${grunk_SOURCE_DIR} ${grunk_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
endif()

add_executable(cad_autodiff src/main.cpp)
target_link_libraries(cad_autodiff PRIVATE grunk geoml)