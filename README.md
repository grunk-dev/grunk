# Grunk

**Disclaimer:** *This is work in progress at a very early stage. Most features have not been implemented yet. Expect the code and API to change frequently!*

Grunk is a parametric modeling engine that helps you build complex models from parameters, track the dependencies of your model features and annotate them with metadata. Features are evaluated lazily and they will be automatically invalidated if any of the features and parameters it depends on changes. You can write your model to a human-readable file to disk and rebuild your model from the saved file. Grunk is targeted at - but not limited to - geometric modeling.

The only assumption grunk makes is that the model consists of certain **features** *(objects of a specific type)* which can be computed with the help of given **functions** from other features or parameters. These functions are expected to be [referentially transparent](https://en.wikipedia.org/wiki/Referential_transparency) *(disregarding logging etc.)*. A model can therefore be identified with a *directed acyclic graph* of functions, their inputs and their outputs.

Functions and feature types are loaded at runtime from grunk plugins. By doing this, each grunk plugin provides some domain specific building blocks for any kind of model.

This way, models built with grunk are highly modular and extendable. Users can share plugins and enrich their models without the need to re-compile anything.


## Installation

Grunk can be installed using [conan](https://conan.io/).

### Install conan

Install conan via pip or conda, whichever you prefer. As always, it is recommended to install python packages in an isolated environment. To install `conan` into an environment called `paradigms` using conda enter the following commands

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
  Note, that conan doesn't store your credentials outside of your current session. If you cannot install anything from the gitlab package registry, it is likely that you have to re-enter this command.

### Install grunk

Now, `grunk` can be installed from the conan remote `gitlab` via

```
conan install grunk/0.1@paradigms/testing --build=missing
```

## Building grunk from source

You need cmake as well as a C++17 compliant compiler to build grunk from source. In addition, grunk depends on the packages `reflect` and `parametric` which can be installed using conan. Make sure you setup conan to use the paradigms gitlab package registry, see the **Installation** section [above](https://gitlab.dlr.de/paradigms/grunk#setup-conan-to-use-the-paradigms-gitlab-package-registry). 

- If you are using Linux and gcc>=5.1, you should [update the ABI used by conan](https://docs.conan.io/en/latest/howtos/manage_gcc_abi.html):
  ```
  conan profile update settings.compiler.libcxx=libstdc++11 default
  ```

- Enter the following commands to install the dependencies and build grunk in debug mode from source using the generator `ninja`:

  ```
  mkdir build && cd build
  conan install .. -r gitlab -s build_type=Debug --build=missing
  cmake .. -DCMAKE_BUILD_TYPE=Debug -DGRUNK_TESTS=ON -GNinja
  ninja
  ```

- Make sure everything works by running the unit tests. 

  - Set the paths to all required shared libraries. The easiest way to do this is using a conan virtual run environment. Assuming you are using Linux, enter:

    ```
    conan install .. --generator=virtualrunenv -s build_type=Debug
    chmod a+x activate_run.sh
    ./activate_run.sh
    ```
  
    On Windows you can skip the `chmod` step and you would execute `activate_run.ps` or `activate_run.bat` in the following step.
  
  - Now run the unit tests:

    ```
    ./tests/runUnitTests
    ```

## Design ideas

### Runtime Objects and Runtime Functions

C++ is a statically typed language and all types must be determined at compile time. To be able to load user defined feature types at runtime and do something with them, grunk works with type-erased objects called `RuntimeObject` as well as `RuntimeFunction`s that accept and return `RuntimeObject`s. In a way, `RuntimeObject`s and `RuntimeFunction`s provide a minimalistic dynamically typed sublanguage in C++. 

Basically, a `RuntimeObject` is an [`std::any`](https://en.cppreference.com/w/cpp/utility/any) on steroids: In addition to the type-erased object, it stores a type descriptor with some functionality for accessing constructors as well as data members and member functions. This type descriptor must be created once by *reflecting* the actual C++ type. This is done at compile time of the plugin that provides the correspong feature type. For this, grunk uses the [reflect](https://gitlab.dlr.de/paradigms/reflect) library.

A `RuntimeFunction` creates a function accepting and returning `RuntimeObject`s from any function accepting and returning types, which have previously been *reflected*. 

Why not use a dynamically typed scripting language like python from the start? Firstly, python would be a big dependency for grunk and would hinder the integrability. Most features of Python are not needed for grunk. It should be easy to use grunk as a fairly light-weight C++ library. Secondly, both the `RuntimeObject`s and python's ability to be dynamically typed come at a performance overhead because types must be resolved at runtime via type-erasure and runtime polymorphism techniques. Using a minimalistic sublanguage in C++ gives us fine-grained control over just how much dynamic typing we need.

### Features and Algorithms

As a user of grunk, you will work with `Feature`s and `Algorithm`s rather than `RuntimeObject`s and `RuntimeFunction`s. 

A `Feature` is a node in a *feature tree*, which wraps an instance of a `RuntimeObject`. `Feature`s can be independent - in which case they serve as input parameters of your model, or they can be the result of `Algorithm`s.

An `Algorithm` wraps a `RuntimeFunction`. `Algorithm`s can be applied to `Feature`s just like a functions can be applied to its inputs. The main difference is, that when the `Algorithm` is applied, no calculation is performed. Instead, the data dependencies between the `Algorithm`s input `Feature`s and output `Feature`s are registered together with the actual computation, that the algorithm performs. This computation is delayed until the value of one of the `Algorithm`s output `Feature` gets explicitly queried. Then, the calculation result is cached until in the `Feature` gets invalidated. This happens, once one of its dependent `Feature`s down the feature tree gets invalidated, e.g. by a change of a root `Feature`.

The DAG management of `Feature`s and `Algorithm`s uses the [parametric](https://gitlab.dlr.de/paradigms/parametric) library.

## Roadmap

 - [ ] grunk is a C++ library and plugins can be written in C++. 
   The usage of features and functions from different plugins is demonstrated and basic file i/o works. plugins can depend on each other
 - [ ] A basic geometric modeling plugin exists
 - [ ] grunk supports basic metadata annotation
 - [ ] language bindings for grunk, e.g. with swig. All functionality is demonstrated in python. C++ plugins can be used from Python without the need to provide language bindings per plugin
 - [ ] It is possible to write plugins in Python, which are also usable from C++
 - [ ] A front end for grunk together with geometric modeling plugin excists, which includes visualization of the geometric models
