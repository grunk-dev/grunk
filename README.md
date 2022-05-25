# Grunk

**Disclaimer:** *This is work in progress at a very early stage. Most features have not been implemented yet. Expect the code and API to change frequently!*

Grunk is a parametric modeling engine that helps you build complex models from parameters, track the dependencies of your model features and annotate them with metadata. Features are evaluated lazily and they will be automatically invalidated if any of the features and parameters it depends on changes. You can write your model to a human-readable file to disk and rebuild your model from the saved file. Grunk is targeted at - but not limited to - geometric modeling.

The only assumption grunk makes is that the model consists of certain **features** *(objects of a specific type)* which can be computed with the help of given **functions** from other features or parameters. These functions are expected to be [referentially transparent](https://en.wikipedia.org/wiki/Referential_transparency) *(disrregarding logging etc.)*. A model can therefore be identified with a *directed acyclic graph* of functions, their inputs and their outputs.

Functions and feature types are loaded at runtime from grunk plugins. By doing this, each grunk plugin provides some domain specific building blocks for any kind of model.

This way, models built with grunk are highly modular and extendable. Users can share plugins and enrich their models without the need to re-compile anything.


## Installation

Grunk can be installed using [conan](https://conan.io/).

### Install conan

Install conan via pip or conda, whichever you prefer. As always, it is recommended to install python packages in an isolated environment. To install `conan` into an environment called `paradigms` enter the following commands

```
conda create -n paradigms conan
conda activate paradigms
```

###  Setup conan to use the paradigms gitlab package registry

- Add the paradigms gitlab package registry and name it `gitlab`
  ```
  conan remote add gitlab https://gitlab.dlr.de/api/v4/projects/21487/packages/conan
  ```
- The package registry is private, so to install packages from it, we need to create a personal access token. When you are logged in to Gitlab click your profile picture at the top right and select `Edit profile`. At the left click `Access Tokens` and create a new token with scope `api`. Copy the token and enter the following commands to use this access token for the remote `gitlab`:
  ```
  conan user <gitlab_username> -r gitlab -p <personal_access_token>
  ```

### Install grunk

Now, `grunk` can be installed from the conan remote `gitlab` via

```
conan install grunk/0.1@paradigms/testing
```

## Building grunk from source

You need cmake as well as a C++17 compliant compiler to build grunk from source. In addition, grunk depends on the packages `reflect` and `parametric` which can be installed using conan. Make sure you setup conan to use the paradigms gitlab package registry, see the **Installation** section [above](https://gitlab.dlr.de/paradigms/grunk#setup-conan-to-use-the-paradigms-gitlab-package-registry). 

Enter the following commands to install the dependencies and build grunk in debug mode from source using the generator `ninja`:

```
mkdir build && cd build
conan install .. -r gitlab -s build_type=Debug
cmake .. -DCMAKE_BUILD_TYPE=Debug -DGRUNK_TESTS=ON -GNinja
ninja
```

Make sure everything works by running the unit tests. 

- Set the paths to all required shared libraries. The easiest way to do this is using a conan virtual run environment. Assuming you are using Linux, enter:

  ```
  conan install .. --generator=virtualrunenv -s build_type=Debug
  chmod a+x activate_run.sh
  ./activate_run.sh
  ```

  On Windows you can skip the `chmod` step and you would execute `activate_run.ps` in the following step.

- Now run the unit tests:

  ```
  ./tests/runUnitTests
  ```

## Design ideas

### Runtime Objects and Runtime Functions

C++ is a statically typed language and all types must be determined at compile time. To be able to load user defined feature types at runtime and do something with them, grunk works with type-erased objects called `RuntimeObject`. Basically, a `RuntimeObject` is an [`std::any`](https://en.cppreference.com/w/cpp/utility/any) on steroids: In addition to the type-erased object, it stores a type description with some functionality for accessing constructors as well as data members and member functions. This type descriptor must be created once, e.g. by a plugin author by *reflecting* the type at compile time of the plugin when registering the feature types provided by the plugin.

A `RuntimeFunction` creates a function accepting and returning `RuntimeObject`s from any function accepting and returning types, which have been *reflected*.

Thus, `RuntimeFunction`s and `RuntimeObject`s provide a minimalistic dynamically typed sublanguage in C++. Why not use a dynamically typed scripting language like python from the start? Firstly, python would be a big dependency for grunk and would hinder the integrability. Most features of Python are not needed for grunk. It should be easy to use grunk as a fairly light-weight C++ library. Secondly, both the `RuntimeObject`s and python's ability to be dynamically typed come at a performance overhead because types must be resolved at runtime via type-erasure and runtime polymorphism techniques. Using a minimalistic sublanguage in C++ gives us fine-grained control over just how much dynamic typing we need.

### Features and Algorithms

TODO

## Roadmap

 - [ ] grunk is a C++ library and plugins can be written in C++. 
   The usage of features and functions from different plugins is demonstrated and basic file i/o works. plugins can depend on each other
 - [ ] A basic geometric modeling plugin exists
 - [ ] grunk supports basic metadata annotation
 - [ ] language bindings for grunk, e.g. with swig. All functionality is demonstrated in python. C++ plugins can be used from Python without the need to provide language bindings per plugin
 - [ ] It is possible to write plugins in Python, which are also usable from C++
 - [ ] A front end for grunk together with geometric modeling plugin excists, which includes visualization of the geometric models
