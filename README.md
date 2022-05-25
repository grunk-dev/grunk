# Grunk

**Disclaimer:** *This is work in progress at a very early stage. Most features have not been implemented yet. Expect the code and API to change frequently!*

Grunk is a parametric modeling engine that helps you build complex models from parameters, track the dependencies of your model features and annotate them with metadata. You can write your model to a human-readable file to disk and rebuild your model from the saved file. Grunk is targeted at - but not limited to - geometric modeling.

The only assumption grunk makes is that the model consists of certain **features** *(objects of a specific type)* which can be computed with the help of given **functions** from other features or parameters. These functions are expected to be [referentially transparent](https://en.wikipedia.org/wiki/Referential_transparency) *(disrregarding logging etc.)*. A model can therefore be identified with a *directed acyclic graph* of functions, their inputs and their outputs.

Functions and feature types are loaded at runtime from grunk plugins. By doing this, each grunk plugin provides some domain specific building blocks for any kind of model.

This way, models built with grunk are highly modular and extendable. Users can share plugins and enrich their models without the need to re-compile anything.


## Installation

Grunk can be installed using [conan](https://conan.io/).

- Install conan via pip or conda, whichever you prefer. As always, it is recommended to install python packages in an isolated environment. To install `conan` into an environment called `paradigms` enter the following commands
  ```
  conda create -n paradigms conan
  conda activate paradigms
  ```
- Add the conan package registry and name it `gitlab`
  ```
  conan remote add gitlab https://gitlab.dlr.de/api/v4/projects/21487/packages/conan
  ```
- The package registry is private, so to install packages from it, we need to create a personal access token. When you are logged in to Gitlab click your profile picture at the top right and select `Edit profile`. At the left click `Access Tokens` and create a new token with scope `api`. Copy the token and enter the following commands to use this access token for the remote `gitlab`:
  ```
  conan user <gitlab_username or deploy_token_username> -r gitlab -p <personal_access_token or deploy_token>
  ```
- Now, `grunk` can be installed from the conan remote `gitlab` via
  ```
  conan install grunk/0.1@paradigms/testing
  ```

## Building

All you need is a C++17 compliant compiler. grunk depends on the packages `reflect` and `parametric` which can be installed using conan. Make sure you setup conan to use the paradigms package registry, see the **Installation** section. 

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

## Roadmap

 - [ ] grunk is a C++ library and plugins can be written in C++. 
   The usage of features and functions from different plugins is demonstrated and basic file i/o works. plugins can depend on each other
 - [ ] A basic geometric modeling plugin exists
 - [ ] grunk supports basic metadata annotation
 - [ ] language bindings for grunk, e.g. with swig. All functionality is demonstrated in python. C++ plugins can be used from Python without the need to provide language bindings per plugin
 - [ ] It is possible to write plugins in Python, which are also usable from C++
 - [ ] A front end for grunk together with geometric modeling plugin excists, which includes visualization of the geometric models
