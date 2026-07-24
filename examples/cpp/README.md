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

Each example is standalone with its own `pixi.toml` and `CMakeLists.txt`:

```bash
cd examples/cpp/cad_autodiff
pixi run all
```

Or step by step:

```bash
pixi install              # Install dependencies
pixi run configure        # Configure with CMake
pixi run build            # Build
pixi run run              # Run
```

## Adding a New Example

1. Create a new directory under `examples/cpp/<example-name>/`
2. Add a `CMakeLists.txt` that uses FetchContent for grunk and geoml
3. Create a `pixi.toml` with example-specific dependencies
4. Add `src/main.cpp` with your example code
5. Add a `README.md` with usage instructions

## Dependencies

Each example fetches the following via CMake FetchContent:

- **grunk**: The grunk library itself
- **geoml**: Geometric modeling library

Other dependencies (e.g., opencascade) are installed via pixi/conda.