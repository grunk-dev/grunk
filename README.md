# Grunk

**Disclaimer:** This is work in progress at a very early stage.

Grunk is a parametric modeling engine that helps you build complex models from parameters, track the dependencies of your model features and annotate them with metadata. You can write your model to a human-readable file to disk and rebuild your model from the saved file. Grunk is targeted at - but not limited to - geometric modeling.

The only assumption grunk makes is that the model consists of certain **features** *(objects of a specific type)* which can be computed with the help of given **functions** from other features or parameters. These functions are expected to map inputs to outputs without any side effects. A model can therefore be identified with a *directed acyclic graph*.

Functions and feature types are loaded at runtime from grunk plugins. Each grunk plugin provides some domain specific building blocks for any kind of model.

This way, models built with grunk are highly modular and extendable. Users can share plugins and enrich their models without the need to re-compile anything.


## Installation

Grunk can be built or installed by conan

TODO

## Building

Grunk has the following build dependencies:

 - boost (headers only)
 - parametric
 - reflect

Both parametric and reflect were written specifically for grunk. You will also need a C++17 compliant compiler. The easiest way to install the dependencies is using conan. Make sure you have local conan packages for parametric and reflect, as they are currently not hosted in a package registry.

```
mkdir build && cd build
conan install ..
cmake .. -DGRUNK_TESTS=ON -GNinja
ninja
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
 - [ ] language bindings for grunk, e.g. with swig. All functionality is demonstrated in python. C++ plugins can be used from Python.
 - [ ] It is possible to write plugins in Python, which are also usable from C++
 - [ ] Provide a front end for grunk together with geometric modeling plugin including visualization