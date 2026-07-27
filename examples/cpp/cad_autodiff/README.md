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
2. **CAD only** (`write_cad_recipe(false)` / `read_cad_recipe`) - a recipe built on
   [geoml](https://github.com/DLR-SC/geoml)/OCCT geometry (a single bezier curve - this is a dev
   artifact, not a shape worth modeling for its own sake), using plain `double`s - no AD. Shows
   that grunk's plugin mechanism can wrap a genuine, non-scripting-oriented C++ library, not just
   something already Lua-friendly like a SWIG-Lua module.
3. **CAD + AD** (`write_cad_recipe(true)` / `read_recipe_ad` - not yet callable, see "Stage 3"
   below) - the *exact same* recipe text as stage 2 (`write_cad_recipe`'s `with_ad` parameter
   changes only which plugin gets loaded, never the recipe script), but now producing AD-carrying
   geometry. This is the payoff - stages 1 and 2 combined in one recipe - and the reason this
   whole example is named "cad_autodiff".

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

### Stage 3: CAD + AD (scaffolding only, not yet built or verified)

`plugins/geoml_adolc/` contains a source file and a standalone CMake project for a
`geoml_adolc_plugin.so`, the AD-enabled counterpart to `geoml_plugin.so` (stage 2). This has
**not** been built or run - it depends on integrating two prototypes that don't currently talk to
each other, and this section documents what that integration still requires rather than a
tested recipe.

**How it's meant to work.** [geoml's `feature/autodiff`
branch](https://github.com/DLR-SC/geoml/tree/feature/autodiff) replaces `double` with
`Standard_Real` consistently throughout geoml's geometry/algorithm layer, and adds a
`GEOML_USE_ADOLC` CMake option. It's built to compile against
[adOCCT](https://github.com/dlr-sp/adOCCT), a fork of OCCT itself that redefines
`Standard_Real` (`src/Standard/Standard_TypeDef.hxx`) to be `Standard_Adouble`, a thin wrapper
around ADOL-C's `adtl::adouble`. With that in place, every OCCT type built on `Standard_Real` -
`gp_Pnt`, `Geom_BezierCurve`, everything - carries an AD tape automatically, with no API changes.
That's why `plugins/geoml_adolc/geoml_adolc_plugin.cpp` `#include`s `../geoml_registration.hpp`
completely unmodified from `geoml_plugin.cpp`: the same registration C++ source, compiled against
a different geoml/OCCT install, is the whole point.

**What's unfinished** (every item below is also marked `TODO(stage3)` at its point in the code -
`grep -rn 'TODO(stage3)'` from `cad_autodiff/` finds all of them):
- **OCCT version mismatch.** geoml's `conanfile.py` pins `opencascade/7.6.2`; adOCCT is forked
  from OCCT V7_9_0. Nobody has reconciled this yet - it needs either building adOCCT at a tag
  closer to 7.6.2 (if adOCCT has one), or repointing geoml's `feature/autodiff` branch at whatever
  OCCT version adOCCT actually forked, and confirming geoml still builds against it.
- **No known-working build of either piece standalone with ADOL-C linked**, let alone the two
  together - both are prototypes (see their own READMEs) with no CI exercising this combination.
- **Getting a *nonzero* derivative out.** Once it builds, the same recipe from stage 2 will run
  geometry construction through `adouble`s, but the recipe as written (`X = grunk.feature(1.)`)
  never *seeds* a derivative direction, so every resulting derivative would be trivially zero.
  Stage 1's `me.seed(x_val)` (`ret:setADValue(0,1.)`) is the pattern to adapt - working that into
  the geoml recipe (or the plugin) without it stopping being "the same recipe" is open.
- **No way yet to pull a derivative back out of a `Geom_BezierCurve`** - needs another registered
  function alongside `export_brep`, e.g. sampling a curve point's coordinate and its derivative.

Both of the last two are tagged `TODO(stage3)` right in `read_recipe_ad` (`src/main.cpp`).

**Roughly, what building it would involve**, once the version mismatch above is sorted out:

1. Build and install adOCCT with `-DUSE_ADOLC=ON` (and `-DADOLC_REVERSE_MODE=OFF` for tapeless/
   forward mode, matching the `adtl::adouble` used elsewhere in this example), pointing it at the
   same ADOL-C install used for stage 1 (`ADOLC_PREFIX` above). See adOCCT's own README for its
   OCCT build prerequisites.
2. Build and install geoml's `feature/autodiff` branch with `-DGEOML_USE_ADOLC=ON` and
   `-DGEOML_USE_ADOLC_MODE=forward` (matching adOCCT's `ADOLC_REVERSE_MODE=OFF` above - the two
   must agree), pointed (via `CMAKE_PREFIX_PATH`/conan) at the adOCCT install from step 1 instead
   of stock OpenCASCADE. Call the install prefix `GEOML_AUTODIFF_PREFIX`.
3. Configure and build the plugin as its own project (see `plugins/geoml_adolc/CMakeLists.txt`
   for why this is separate from the main `cad_autodiff` CMake project):
   ```bash
   cd examples/cpp/cad_autodiff/plugins/geoml_adolc
   cmake -S . -B build -D GEOML_AUTODIFF_DIR=GEOML_AUTODIFF_PREFIX
   cmake --build build
   ```
4. Point the `GEOML_ADOLC_PLUGIN_SO_PATH` CMake cache variable in the main project
   (`../../CMakeLists.txt`) at the `.so` built in step 3, reconfigure/rebuild `cad_autodiff`, and
   uncomment the `read_recipe_ad();` call at the bottom of `main()` (`src/main.cpp`, tagged
   `TODO(stage3)`) - it reuses `bezier_curve.grr.yml` from stage 2, no separate AD write needed.
   `read_recipe_ad` still has its own `TODO(stage3)`s to resolve (see above).

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
| adOCCT, geoml `feature/autodiff` (stage 3 only, not yet built) | [dlr-sp/adOCCT](https://github.com/dlr-sp/adOCCT), [DLR-SC/geoml@feature/autodiff](https://github.com/DLR-SC/geoml/tree/feature/autodiff) - build manually, see "Stage 3" above |

## Project Structure

```
cad_autodiff/
├── CMakeLists.txt              # Build configuration with FetchContent (stages 1-2)
├── pixi.toml                   # Dependency management
├── README.md                   # This file
├── plugins/
│   ├── occt_sol_traits.hpp     # sol2 traits for OCCT Handle(T) - used only by geoml_registration.hpp
│   ├── geoml_registration.hpp  # register_geoml (incl. export_brep), shared verbatim by both geoml plugins below
│   ├── geoml/
│   │   └── geoml_plugin.cpp        # Stage 2 plugin (no AD) - built as geoml_plugin.so, dlopen'd by main.cpp
│   └── geoml_adolc/
│       ├── CMakeLists.txt          # Standalone project - see README's "Stage 3" section
│       └── geoml_adolc_plugin.cpp  # Stage 3 plugin (AD) - scaffolding only, not yet built/verified
└── src/
    └── main.cpp                # Example source code - deliberately never includes an OCCT/geoml header
```

All OCCT/geoml-specific code is confined to `plugins/` - `src/main.cpp` only ever deals with
`grunk::state`, recipes, and generic `sol::object`/`bool` values, even when verifying a curve got
built (via `export_brep`, called from within the recipe, not C++ code reaching into a
`Handle(Geom_BezierCurve)`). A real grunk plugin should be the only place that needs to know a
specific CAD/geometry library exists at all.
