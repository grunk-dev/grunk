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
- ADOL-C 2.7.2

### Build and install ADOL-C

Clone and checkout 2.7.2:

```bash
cd examples/cpp/cad_autodiff
mkdir -p build/_deps
cd build/_deps
git clone https://github.com/coin-or/ADOL-C.git
cd ADOL-C
git checkout releases/2.7.2
```

Create ADOL-C installation directory, e.g., inside the ADOL-C directory:

```bash
mkdir adolc_base
```

and copy its the absolute path, abbreviated as `ADOLC_PREFIX` in the following.

Configure, build and install ADOL-C:

```bash
autoreconf -fi
./configure --prefix=ADOLC_PREFIX --enable-atrig-erf --with-boost=no
make -j
make install
```

As a result, the `ADOLC_PREFIX` directory should contain the headers and the library of ADOL-C.

In case your preferred `ADOLC_PREFIX` path is different than the one suggested here,
 additional steps are required.
- Go back to the `cad_autodiff` directory.
- Put the absolute or relative path to `ADOLC_PREFIX` inside the `pixi.toml` file.
  That is, look for the line:
```
    "-D", "ADOLC_BASE_DIR=build/_deps/ADOL-C/adolc_base"
```
And replace `build/_deps/ADOL-C/adolc_base` with your actual path.

## Building and Running

Go to the `cad_autodiff` directory if not already done so.

From there:

```bash
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
| ADOL-C | From [GitHub](https://github.com/coin-or/ADOL-C.git) |
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
