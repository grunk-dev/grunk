<!--
SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>

SPDX-License-Identifier: MPL-2.0
-->

# CAD Autodiff Example

This example demonstrates using grunk with [geoml](https://github.com/DLR-SC/geoml) for CAD modeling and automatic differentiation.

## Prerequisites

- CMake >= 3.15
- C++17 compiler
- [pixi](https://pixi.sh/) (recommended) or conda

## Building and Running

```bash
cd examples/cpp/cad_autodiff
pixi run all
```

Or step by step:

```bash
pixi install              # Install dependencies
pixi run configure        # Configure the CMake build
pixi run build            # Build the example
pixi run run              # Run the executable
```

## Dependencies

| Dependency | Source |
| -- | -- |
| grunk | FetchContent from GitHub |
| geoml | FetchContent from GitHub |
| opencascade | conda-forge/dlr-sc |
| libparametric, yaml-cpp, sol2, lua | conda-forge |

## Project Structure

```
cad_autodiff/
├── CMakeLists.txt    # Build configuration with FetchContent
├── pixi.toml         # Dependency management
├── README.md         # This file
└── src/
    └── main.cpp      # Example source code
```