<!--
SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>

SPDX-License-Identifier: MPL-2.0
-->

# C++ Examples

This directory contains C++ example applications demonstrating grunk's capabilities.

## Available Examples

| Example | Description |
| -- | -- |
| [cad_autodiff](cad_autodiff/) | CAD modeling with automatic differentiation using geoml |

## Building Examples

### Standalone Mode (Recommended)

Each example has its own `pixi.toml` and can be built independently:

```bash
cd examples/cpp/cad_autodiff
pixi run all
```

Or step by step:

```bash
pixi install                # Install dependencies
pixi run configure          # Configure with CMake
pixi run build              # Build
pixi run run                # Run the executable
```

### Integrated Mode

Build examples as part of the grunk repository:

```bash
# From grunk repository root
pixi run configure --examples ON
pixi run build
./build/examples/cpp/cad_autodiff/cad_autodiff
```

## Adding a New Example

1. Create a new directory under `examples/cpp/<example-name>/`
2. Add a `CMakeLists.txt` that:
   - Uses FetchContent for geoml
   - Detects if `TARGET grunk` exists (integrated mode)
   - Links against grunk and geoml
3. Create a `pixi.toml` with example-specific dependencies (e.g., opencascade)
4. Add `src/main.cpp` with your example code
5. Add a `README.md` with usage instructions

### Example CMakeLists.txt Template

```cmake
cmake_minimum_required(VERSION 3.15)
project(my_example LANGUAGES CXX)

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
    FetchContent_Populate(geoml)
    add_subdirectory(${geoml_SOURCE_DIR} ${geoml_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET grunk)
    FetchContent_Declare(grunk
        GIT_REPOSITORY https://github.com/grunk-dev/grunk.git
        GIT_TAG main
    )
    FetchContent_GetProperties(grunk)
    if(NOT grunk_POPULATED)
        FetchContent_Populate(grunk)
        set(GRUNK_WITH_TASKFLOW OFF CACHE BOOL "" FORCE)
        set(GRUNK_WITH_DYNAMIC ON CACHE BOOL "" FORCE)
        set(GRUNK_WITH_RECIPE ON CACHE BOOL "" FORCE)
        include_directories(${grunk_BINARY_DIR})
        add_subdirectory(${grunk_SOURCE_DIR} ${grunk_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
endif()

add_executable(my_example src/main.cpp)
target_link_libraries(my_example PRIVATE grunk geoml)
```

### Example pixi.toml Template

```toml
[workspace]
name = "grunk-my-example"
version = "0.1.0"
description = "My example for grunk"
platforms = ["linux-64"]
channels = ["conda-forge"]

[target.linux-64.dependencies]
cmake = "*"
ninja = "*"
libparametric = "*"
yaml-cpp = "*"
sol2 = "*"
lua = "*"
opencascade = "==7.6.2"

[tasks]
configure = { cmd = ["cmake", "-G", "Ninja", "-S", ".", "-B", "build", "-D", "CMAKE_BUILD_TYPE={{ config }}"], args = [{ "arg" = "config", "default" = "Release" }] }
build = { cmd = "cmake --build . --parallel", cwd = "build" }
run = { cmd = "./my_example", cwd = "build" }
all = { depends-on = ["configure", "build", "run"] }
```

## Dependencies

- **geoml**: Fetched via FetchContent from GitHub
- **grunk**: Fetched via FetchContent (standalone) or linked from grunk build (integrated)
- **opencascade**: Installed via conda package
- Other dependencies: libparametric, yaml-cpp, sol2, lua (installed via pixi)