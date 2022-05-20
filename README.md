# Grunk

**Disclaimer:** This is work in progress.

Grunk is a parametric modeling engine that helps you build complex models from parameters, track the dependencies of your model features and annotate them with metadata.

The assumption behind grunk is that the model consists of **features** *(objects of a specific type)* which can be computed with the help of **functions** from other features or parameters. All functions and features are loaded at runtime from grunk plugins. This way, models built with grunk are highly modular and extendable. Users can share plugins and enrich their models without the need to re-compile.

Grunk is targeted at - but not limited to - geometric modeling.

## Installation

Grunk can be built by conan

```
conan create .
```

## Building

Grunk has the following build dependencies.

 - boost
 - range-v3
 - parametric
 - reflect

Both parametric and reflect were written specifically for grunk. The easiest way to install the dependencies is using conan.

```
mkdir build && cd build
conan install ..
cmake .. -DGRUNK_TESTS=ON
make -j
```

Make sure everything works:

```
cd tests
./runUnitTests
```

## Roadmap

 - [ ] grunk is a C++ library and plugins can be written in C++. 
   The usage of features and functions from different plugins is demonstrated and basic file i/o works. plugins can depend on each other.
 - [ ] A basic geometric modeling plugin exists
 - [ ] grunk supports basic metadata annotation
 - [ ] language bindings for grunk, e.g. with swig. All functionality is demonstrated in python.
 - [ ] Provide a front end for grunk together with geometric modeling plugin including visualization