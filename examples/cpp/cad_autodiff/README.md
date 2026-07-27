<!--
SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>

SPDX-License-Identifier: MPL-2.0
-->

# CAD Autodiff Example

This example's point: algorithmic differentiation (AD) is possible *through* grunk's dynamic
layer, not just around it - a value can flow through a grunk recipe (dependency tracking, lazy
evaluation, YAML serialization) and still carry its AD tape/derivative information. It builds
this up in three stages, run in order by `main()`:

1. **AD only** (`write_autodiff_only_recipe` / `read_autodiff_only_recipe`) - a recipe built
   purely on [ADOL-C](https://github.com/coin-or/ADOL-C)'s `adouble` type, no CAD geometry
   involved. Shows that grunk's dynamic layer is transparent to an AD type.
2. **CAD only** (`write_cad_only_recipe` / `read_cad_only_recipe`) - a recipe built on
   [geoml](https://github.com/DLR-SC/geoml)/OCCT geometry, using plain `double`s - no AD. Shows
   that grunk's plugin mechanism can wrap a genuine, non-scripting-oriented C++ library, not just
   something already Lua-friendly like a SWIG-Lua module.
3. **CAD + AD** (planned follow-up, not yet implemented here) - the *same* CAD recipe from stage
   2, but with `geoml_plugin.so` swapped for a `geoml_adolc_plugin.so` built against ADOL-C,
   differentiating the geometry construction itself. This is the payoff - stages 1 and 2 combined
   in one recipe - and the reason this whole example is named "cad_autodiff".

Stages 1 and 2 each mock up a different kind of grunk plugin from
[grunk-dev/grunk#235](https://github.com/grunk-dev/grunk/issues/235): `load_adolc_plugin` loads
`adtl.so`, a compiled Lua module (SWIG-generated), while `load_geoml_plugin` loads
`geoml_plugin.so` (see `plugins/geoml/geoml_plugin.cpp`), a plain C++ plugin whose entry point
registers types/functions directly against a `grunk::state`. Both are `dlopen`'d at runtime
rather than linked in, so only their path needs to be known at `cad_autodiff`'s build time, not
the plugin's own code - a stand-in for grunk's still-unwritten runtime plugin loader.

Note that this is a bare-bone example that requires building C++ code from scratch. In the current state, it is a proof-of-concept. This will be simplified in the future, once we provide grunk as a conda-forge package and have a means to distribute binary grunk plugins.

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

### Build and install the grunk adolc interface

We need a grunk plugin that pulls in the adouble data type from ADOL-C. We can generate the LUA code using a swig interface file. The code for this is provided in https://gitlab.dlr.de/dlr-sp/occt-differentiation/swig-adol-c.git. 

```bash
cd examples/cpp/cad_autodiff
mkdir -p build/_deps
cd build/_deps
git clone https://gitlab.dlr.de/dlr-sp/occt-differentiation/swig-adol-c.git
cd swig-adol-c
git checkout lua_wrapper
```

Now build the Lua interface. We need to tell cmake where to find ADOL-C as well as LUA.
ADOL-C has been installed in the previous step, LUA will be installed as part of the pixi installation. 

`PIXI_ENV` is the location of the default pixi environment. Usually, this is `.pixi/envs/default` relative to the example directory.

```bash
pixi install
mkdir build && cd build
cmake -DADOLC_INCLUDE_DIR=ADOLC_PREFIX/include -DADOLC_LIB_DIR=ADOLC_PREFIX/lib64 -DCMAKE_PREFIX_PATH=PIXI_ENV ..
make
```

As a result, one should see the `adtl.so` file. `main.cpp` loads this file at runtime into the
grunk::state's Lua interpreter (via `grunk::state::load_compiled_plugin`, see `load_adolc_plugin`),
rather than including ADOL-C's headers directly.

If `adtl.so` ends up somewhere other than `build/_deps/swig-adol-c/build` (relative to the
`cad_autodiff` directory), put the absolute or relative path inside the `pixi.toml` file, the same
way as for `ADOLC_BASE_DIR` above. Look for the line:
```
    "-D", "ADTL_LIB_DIR=build/_deps/swig-adol-c/build"
```
And replace `build/_deps/swig-adol-c/build` with your actual path.


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
| swig-adol-c | Install manually from https://gitlab.dlr.de/dlr-sp/occt-differentiation/swig-adol-c, DLR inner source |
| grunk | FetchContent from GitHub |
| geoml | FetchContent from GitHub |
| opencascade | conda-forge/dlr-sc |
| libparametric, yaml-cpp, sol2, lua | conda-forge |

## Project Structure

```
cad_autodiff/
├── CMakeLists.txt         # Build configuration with FetchContent
├── pixi.toml              # Dependency management
├── README.md              # This file
├── include/
│   └── occt_sol_traits.hpp  # sol2 traits for OCCT Handle(T), shared by main.cpp and the geoml plugin
├── plugins/
│   └── geoml/
│       └── geoml_plugin.cpp # Mockup of a native C++ grunk plugin (built as geoml_plugin.so, dlopen'd by main.cpp)
└── src/
    └── main.cpp           # Example source code
```
