<!--
SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>

SPDX-License-Identifier: MPL-2.0
-->

# CAD Autodiff Example

This example demonstrates using grunk with [geoml](https://github.com/DLR-SC/geoml) for CAD modeling and automatic differentiation. It shows how to combine parametric modeling with geometric operations.

## Prerequisites

- CMake >= 3.15
- C++17 compiler
- [pixi](https://pixi.sh/) (recommended) or conda

## Building and Running

### Quick Start

```bash
cd examples/cpp/cad_autodiff
pixi run all
```

### Step by Step

```bash
pixi install              # Install dependencies
pixi run configure        # Configure the CMake build
pixi run build            # Build the example
pixi run run              # Run the executable
```

### Available Tasks

| Task | Description |
| -- | -- |
| `pixi run configure` | Configure the CMake build |
| `pixi run build` | Build the example |
| `pixi run run` | Run the executable |
| `pixi run all` | Configure, build, and run |

## Building as Part of grunk

From the grunk repository root:

```bash
pixi run configure Release --examples ON
pixi run build
./build/examples/cpp/cad_autodiff/cad_autodiff
```

## Dependencies

| Dependency | Source | Notes |
| -- | -- | -- |
| opencascade | conda-forge/dlr-sc | CAD kernel |
| geoml | FetchContent | Geometric modeling library |
| grunk | FetchContent or integrated | Parametric modeling |
| libparametric | conda-forge | Dependency for grunk |
| yaml-cpp | conda-forge | Dependency for grunk recipe |
| sol2, lua | conda-forge | Dependencies for grunk dynamic |
| taskflow | conda-forge | Optional, for multithreading |

## Project Structure

```
cad_autodiff/
├── CMakeLists.txt    # Build configuration (dual-mode)
├── pixi.toml         # Dependency management
├── README.md         # This file
└── src/
    └── main.cpp      # Example source code
```

## Dual-Mode Build

The CMakeLists.txt supports two build modes:

1. **Standalone**: When built independently, fetches both grunk and geoml via FetchContent
2. **Integrated**: When `TARGET grunk` exists (built as part of grunk), links against the existing grunk target

This allows the same example code to work in both development and production contexts.