.. SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
..
.. SPDX-License-Identifier: MPL-2.0

*************
Installation
*************

=======================
Installation with conda
=======================

grunk will be published on [conda-forge](https://conda-forge.org/) in the future. In the meantime build and install from source (instructions below) or use the packaged artifacts provided by maintainers.

.. code-block::console 

   # if grunk becomes available on conda-forge in your channel
   conda install grunk


====================
Building from source
====================

This section explains how to configure, build and install grunk from source. The project uses CMake + Ninja; examples below assume a Unix-like shell (bash) and that CMake and Ninja are available. grunk is tested on Linux, MacOS and Windows.

Minimum recommended workflow
---------------------------

1. Install build prerequisites (see next section).
2. Use the provided pixi tasks to configure and build (recommended).
3. Alternatively run cmake manually and build with Ninja.


Layered architecture
=====================

grunk is organised as a set of layered libraries. The following table shows the main components and their purpose:

.. list-table:: Library overview
   :header-rows: 1

   * - Library
     - Description
     - Dependencies
   * - ``grunk::core``
     - Backend for tracking parametric dependencies. Lazy evaluation and automatic invalidation with minimal overhead. Header-only, thin wrapper for parametric
     - ``parametric``. Optional: ``taskflow`` for multithreading support. Enable with the CMake option ``GRUNK_WITH_TASKFLOW=ON``.
   * - ``grunk::dynamic``
     - Dynamic scripting support (Lua). Write scripts without boilerplate that are automatically parametric. Simple serialization/deserialization to/from Lua. Header-only.
     - Depends on ``grunk::core``, ``Lua`` and ``sol2``. Enable with ``-DGRUNK_WITH_DYNAMIC=ON``.
   * - ``grunk::recipe``
     - YAML-based recipes: a human-readable structured exchange format for parametric models.
     - Depends on ``grunk::dynamic`` and ``yaml-cpp``. Enable with ``-DGRUNK_WITH_RECIPE=ON``.
   * - ``grunk::plugins``
     - Plugin support for sharing reusable functions and data types. Plugins can be written in C++ (compiled) or Lua (scripting).
     - Depends on ``grunk::recipe`` (WIP).

The library targets a modular design: the `grunk` target re-exports the enabled components. The CMake options below control which parts are configured and built.

Requirements
============

The project has a small set of build-time and optional runtime dependencies. The core library is header-only but some features require external libraries:

- A C++17 compliant compiler, CMake (>= 3.15) and Ninja for the build system
- Lua and sol2 for the dynamic scripting backend (optional)
- yaml-cpp for recipe support (optional)
- taskflow (optional) for multithreaded execution in `grunk::core`

If you are using conda, a convenient way is to install the development environment via the `pyproject.toml` included in the repository. See the "Using pixi" section below.


CMake configuration options
===========================

The build is controlled by a small number of CMake variables. Pass them to CMake on the command line ("-D<VAR>=<VALUE>") or use the provided pixi tasks which set sane defaults.

- ``GRUNK_WITH_DYNAMIC`` (OFF by default): Build and install the ``grunk::dynamic`` module, enabling the Lua-based dynamic scripting API. Set ``-DGRUNK_WITH_DYNAMIC=ON`` to enable.
- ``GRUNK_WITH_RECIPE`` (OFF/ON as appropriate): Build the YAML recipe support (``grunk::recipe``). When enabled, a binary-compatible library target is created and additional headers are installed; set ``-DGRUNK_WITH_RECIPE=ON``.
- ``GRUNK_WITH_TASKFLOW`` (OFF by default): Enable optional integration with Taskflow for multithreaded execution inside ``grunk::core``. When enabled you must provide Taskflow as a dependency and set ``-DGRUNK_WITH_TASKFLOW=ON``.


Installing requirements (conda)
===============================

We recommend using conda to create an isolated development environment. The project includes a `pyproject.toml` at the workspace root (used by the team's workflow). A minimal conda environment for building can be created with:

.. code-block:: console

   # create and activate a conda env (example)
   conda create -n grunk-dev cmake ninja python -y
   conda activate grunk-dev

If you want the full toolchain (docs tools, reuse, etc.) you can use the `pixi` configuration defined in this repository's `pyproject.toml`. The `pyproject.toml` defines tasks such as `configure`, `build`, `install` and helper environments like `docs` and `tools` (see the project root `pyproject.toml` for the exact task definitions).


Using pixi tasks (recommended)
==============================

The repository defines pixi tasks in `pyproject.toml` (tool.pixi.tasks). If you have the `pixi` CLI installed you can run those tasks directly. The key tasks in `pyproject.toml` are:

- ``configure``: runs CMake with a reasonable set of default arguments to generate a Ninja build directory. The task accepts `config` (Release/Debug) and `docs` flags.
- ``build``: runs `cmake --build .` inside the build directory (uses Ninja by default).
- ``install``: a composite pixi task that installs both the C++ artifacts and the Python package (implemented using `install_cpp` and `install_python`).
- ``test``: a composite task that runs both C++ and Python tests (implemented using `test_cpp` and `test_python`).

Example (using pixi to configure, build and install):

.. code-block:: console

   # configure (defaults: Release, docs OFF)
   pixi r configure

   # build (parallel)
   pixi r build

   # install both C++ and Python parts
   pixi r install

If you don't use `pixi`, you can run the equivalent commands manually:

.. code-block:: console

   cmake -G Ninja -S . -B build -D CMAKE_INSTALL_PREFIX=build/install -D CMAKE_BUILD_TYPE=Release \
         -D GRUNK_WITH_DYNAMIC=ON -D GRUNK_WITH_RECIPE=ON
   cmake --build build -j
   cmake --install build --prefix build/install


Python bindings and extras
==========================

If all requirements are available in the current environment (e.g. installed via pixi), it suffices to run 

.. code-block:: console

  pip install . 

or equivalently via pixi task

.. code-block:: console

  pixi r install_python

Running unit tests
==================

If tests are enabled during configuration they are built into `build/tests`. The pixi `test` task (or running the test binary directly) executes the unit tests. Example:

.. code-block:: console

   # with pixi
   pixi r test

   # or run the test executable
   ./build/tests/runUnitTests


Building this documentation
===========================

The documentation is built using Sphinx. Use the `docs` pixi environment or install the required Python packages (see the `pyproject.toml` `[tool.pixi.feature.docs]` features) and run:

.. code-block:: console

   # activate docs environment (example)
   pixi run --env docs configure Release ON

This confiugres CMake to include the documentation. The docs environment contains the tools required to generate the documentation (sphinx, breathe, doxygen...)
