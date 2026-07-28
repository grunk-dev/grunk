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
3. **CAD + AD** (`write_cad_recipe(true)` / `read_recipe_ad`, see "Stage 3" below) - the *exact
   same* recipe text as stage 2 (`write_cad_recipe`'s `with_ad` parameter changes only which
   plugin gets loaded, never the recipe script), but now producing AD-carrying geometry. This is
   the payoff - stages 1 and 2 combined in one recipe - and the reason this whole example is
   named "cad_autodiff".

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

### Stage 3: CAD + AD

`plugins/geoml_adolc/` contains a source file and a standalone CMake project for a
`geoml_adolc_plugin.so`, the AD-enabled counterpart to `geoml_plugin.so` (stage 2), built and
verified against a real closed-form value (see "Verifying the derivative" below).

**How it works.** [geoml's `feature/autodiff`
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

**What building it actually required**, beyond the "roughly" outline this section used to give:
- **OCCT version mismatch.** geoml's `conanfile.py` pins `opencascade/7.6.2`; adOCCT (`ad/master`)
  forks OCCT V7_9_0. Rather than reconciling the pin itself, geoml's `feature/autodiff` checkout
  was built directly against the adOCCT install (bypassing conan, `CMAKE_PREFIX_PATH` only) and
  the resulting ~8 compile errors were genuine OCCT 7.6→7.9 API drift (a `math_Vector` alias
  change, `TopoDS_Shape`/`TopLoc_Location::HashCode` replaced by `std::hash` specializations, one
  dead `#include "Standard_values.h"`) plus two real gaps in geoml's own AD code (a `getPrimal`
  helper duplicating one adOCCT already provides, an `int`/`float` cast geoml's AD path hadn't
  hit yet). Every fix is in `patches/geoml-feature-autodiff-occt79.patch`, applied to the checkout
  that produced the geoml install this plugin links against - it's not applied by any CMake
  project here.
- **geoml needs `-DCMAKE_POSITION_INDEPENDENT_CODE=ON`.** It ends up statically linked into
  `geoml_adolc_plugin.so`; without it, linking fails needing `-fPIC`.
- **The installed geoml doesn't re-declare its own dependencies.** `geoml-config.cmake` only
  restores the `geoml` target itself - not the `OpenCASCADE`/`ADOLC::ADOLC` targets its link
  interface names by bare target name (`TKernel`, `TKMath`, ..., `ADOLC::ADOLC`). Any consumer
  (this plugin's `CMakeLists.txt`) has to `find_package(OpenCASCADE CONFIG REQUIRED PATHS
  <adOCCT install> NO_DEFAULT_PATH)` and `pkg_check_modules(ADOLC ...)` itself before
  `find_package(geoml)` - `NO_DEFAULT_PATH` matters here specifically, since the pixi environment
  also has stock `opencascade==7.6.2` on `CMAKE_PREFIX_PATH` (needed for Lua/sol2) and would
  otherwise silently win over adOCCT.
- **Getting a *nonzero* derivative out required two things `write_cad_recipe`'s shared recipe text
  can't do on its own**, both handled in `read_recipe_ad` (`src/main.cpp`), not the recipe:
  - *Seeding X.* `gp_Pnt.new(X, 0., 0.)` runs (lazily) against the specific `DynamicFeature` node
    already built for `X` when the recipe was read - reassigning the Lua variable `X` afterwards
    wouldn't reach it. `seed_x` (`geoml_registration.hpp`, adapting stage 1's
    `me.seed(x_val)`/`ret:setADValue(0,1.)`) builds a seeded `Standard_Adouble` from `X`'s current
    (plain-number) value, and `read_recipe_ad` pushes it onto that same node via
    `DynamicFeature::set_value` - the mechanism grunk provides for exactly this: updating a
    feature's value in place, invalidating dependents. One subtlety: `seed_x` is itself a
    tracked/decorated function, so calling it produces another lazy `DynamicFeature`, not the
    `Standard_Adouble` directly - `.value()` on that forces the one evaluation needed.
  - *Reading a derivative back out.* `curve_point_x`/`curve_point_dx_dX`/`curve_point_dy_dX`
    (`geoml_registration.hpp`, AD-only) sample a point on the curve and read its primal value or
    `getADValue(0)`, the same tape direction `seed_x` seeds. `read_recipe_ad` calls them via an
    appended `recipe.eval` (the same pattern `read_cad_recipe` already uses for `export_brep`) -
    this doesn't touch the recipe text shared with stage 2 either.
  - Both `gp_Pnt.new`'s constructor and `curve_point_x`'s `u` parameter needed a mixed
    number/`Standard_Adouble` argument path: a recipe literal like `0.` is a plain Lua number,
    but `Standard_Real` is a real class in the AD build (not a fundamental type), so sol2 won't
    implicitly convert one to the other the way it does for `double`. See the `sol::object`-based
    `to_real` conversion in `gp_Pnt`'s AD constructor overload.

**Verifying the derivative.** At the curve parameter `u=0.5` used in `read_recipe_ad`, the cubic
Bezier weight on `P_1` (the only pole `X` feeds) is `(1-u)^3 = 0.125` - a closed form independent
of grunk/geoml/adOCCT entirely. The example's own output confirms the AD result against it
exactly: `curve_point_x(curve, 0.5)` is `1.625`, `curve_point_dx_dX` is `0.125`, and
`curve_point_dy_dX` (X reaches no other coordinate) is exactly `0` - a leakage sanity check, not
just a magnitude check.

**Building it yourself:**

1. Build and install adOCCT (`ad/master`) with `-DUSE_ADOLC=ON` and `-DADOLC_REVERSE_MODE=OFF`
   (tapeless/forward mode, matching the `adtl::adouble` used elsewhere in this example), pointing
   it at the same ADOL-C install used for stage 1 (`ADOLC_PREFIX` above) via
   `-D3RDPARTY_ADOLC_DIR=ADOLC_PREFIX`. See adOCCT's own README for its OCCT build prerequisites.
   Remember `cmake --build --target install` - the build alone doesn't populate an install
   prefix.
2. Check out geoml's `feature/autodiff` branch, apply
   `patches/geoml-feature-autodiff-occt79.patch`, then configure with `-DGEOML_USE_ADOLC=ON`,
   `-DGEOML_USE_ADOLC_MODE=forward` (matching adOCCT's `ADOLC_REVERSE_MODE=OFF` - the two must
   agree) and `-DCMAKE_POSITION_INDEPENDENT_CODE=ON`, pointed (`CMAKE_PREFIX_PATH`, not conan) at
   the adOCCT install from step 1. `PKG_CONFIG_PATH` needs the ADOL-C install's
   `lib64/pkgconfig` on it for geoml's own `UseADOLC.cmake` to find ADOL-C. Build and install;
   call the install prefix `GEOML_AUTODIFF_PREFIX`.
3. Configure and build the plugin as its own project (see `plugins/geoml_adolc/CMakeLists.txt`
   for why this is separate from the main `cad_autodiff` CMake project), with `PKG_CONFIG_PATH`
   still set as in step 2:
   ```bash
   cd examples/cpp/cad_autodiff/plugins/geoml_adolc
   cmake -S . -B build -D GEOML_AUTODIFF_DIR=GEOML_AUTODIFF_PREFIX -D ADOCCT_DIR=<adOCCT install>
   cmake --build build
   ```
4. `GEOML_ADOLC_PLUGIN_SO_PATH` in the main project (`../../CMakeLists.txt`) already defaults to
   where step 3 puts `libgeoml_adolc_plugin.so` - only override it if yours ends up elsewhere.
   Reconfigure/rebuild `cad_autodiff` and run it; `read_recipe_ad()` is called unconditionally
   from `main()`. The adOCCT/ADOL-C libraries aren't on the runtime linker path by default (`pixi
   run run` doesn't set this) - `libgeoml_adolc_plugin.so` is `dlopen`'d with `RTLD_LOCAL`, so its
   own OCCT/ADOL-C dependencies still need to resolve at runtime:
   ```bash
   export LD_LIBRARY_PATH="<adOCCT install>/lib:<ADOL-C install>/lib64:<pixi env>/lib:$LD_LIBRARY_PATH"
   ```
   Skipping this either fails the `dlopen` outright, or - more dangerous - lets the dynamic linker
   fall back to the stock `opencascade==7.6.2` already in the pixi environment, silently
   reintroducing the version mismatch `NO_DEFAULT_PATH` (step 3's `CMakeLists.txt`) was there to
   rule out at configure time.

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
| adOCCT, geoml `feature/autodiff` (stage 3 only) | [dlr-sp/adOCCT](https://github.com/dlr-sp/adOCCT), [DLR-SC/geoml@feature/autodiff](https://github.com/DLR-SC/geoml/tree/feature/autodiff) - build manually, see "Stage 3" above |

## Project Structure

```
cad_autodiff/
├── CMakeLists.txt              # Build configuration with FetchContent (stages 1-2)
├── pixi.toml                   # Dependency management
├── README.md                   # This file
├── patches/
│   └── geoml-feature-autodiff-occt79.patch  # Applied to the geoml feature/autodiff checkout
│                                             # built for stage 3 - see "Stage 3" section
├── plugins/
│   ├── occt_sol_traits.hpp     # sol2 traits for OCCT Handle(T) - used only by geoml_registration.hpp
│   ├── geoml_registration.hpp  # register_geoml (incl. export_brep and, AD-only, seed_x/
│   │                           # curve_point_x/curve_point_dx_dX/curve_point_dy_dX), shared
│   │                           # verbatim by both geoml plugins below
│   ├── geoml/
│   │   └── geoml_plugin.cpp        # Stage 2 plugin (no AD) - built as geoml_plugin.so, dlopen'd by main.cpp
│   └── geoml_adolc/
│       ├── CMakeLists.txt          # Standalone project - see README's "Stage 3" section
│       └── geoml_adolc_plugin.cpp  # Stage 3 plugin (AD) - built against adOCCT + geoml feature/autodiff
└── src/
    └── main.cpp                # Example source code - deliberately never includes an OCCT/geoml header
```

All OCCT/geoml-specific code is confined to `plugins/` - `src/main.cpp` only ever deals with
`grunk::state`, recipes, and generic `sol::object`/`bool` values, even when verifying a curve got
built (via `export_brep`, called from within the recipe, not C++ code reaching into a
`Handle(Geom_BezierCurve)`). A real grunk plugin should be the only place that needs to know a
specific CAD/geometry library exists at all.
