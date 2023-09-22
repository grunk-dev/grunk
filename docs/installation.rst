.. `toctree`

*************
Installation
*************

Grunk can be installed from the 
`paradigms gitlab package registry <https://gitlab.dlr.de/groups/paradigms/-/packages>`_ 
using `conan <https://conan.io/>`_.

.. _setup-conan:

Setup conan
===========

Install conan
*************

Install ``conan`` via pip or conda, whichever you prefer. 
As always, it is recommended to install python packages in an isolated environment. 
To install ``conan`` into an environment called `paradigms` using conda enter the 
following commands

.. code-block:: console

   conda create -n paradigms conan
   conda activate paradigms

If you are using Linux, you should 
`update the ABI used by conan <https://docs.conan.io/en/latest/howtos/manage_gcc_abi.html>`_:

.. code-block:: console

   conan profile update settings.compiler.libcxx=libstdc++11 default

Setup conan to use the paradigms gitlab package registry
********************************************************

- Add the paradigms gitlab package registry and name it ``gitlab``
  
  .. code-block:: console

     conan remote add gitlab https://gitlab.dlr.de/api/v4/projects/21487/packages/conan
  
- The package registry is private, so to install packages from it, we 
  need to create a personal access token. When you are logged in to Gitlab 
  click your profile picture at the top right and select ``Edit profile``. 
  At the left click ``Access Tokens`` and create a new token with scope
  ``api``. Copy the token and enter the following commands to use this access 
  token for the remote ``gitlab``:

  .. code-block:: console

     conan user <gitlab_username> -r gitlab -p <personal_access_token>
  
  Note, that conan doesn't store your credentials outside of your current 
  session. If you cannot install anything from the gitlab package registry, 
  it is likely that you have to re-enter this command.

Install grunk
=============

Now, ``grunk`` can be installed from the conan remote ``gitlab`` via

.. code-block:: console

    conan install grunk/0.2.1 --build=missing

********************
Building from source
********************

.. _prerequisites:

Requirements
============

You need cmake as well as a C++17 compliant compiler to build grunk from 
source. 

In addition, grunk depends on the packages ``boost``, ``reflect`` and 
``parametric``. 

- `boost <https://www.boost.org/>`_: grunk uses the header-only boost::dll library
  for its plugin system.
- `yaml-cpp <https://github.com/jbeder/yaml-cpp>`_: grunk uses yaml-cpp for reading and writing
  a parametric file in YAML format.
- `reflect <https://gitlab.dlr.de/paradigms/reflect>`_: This is a simple C++ type reflection library used 
  for the dynamic type system in grunk, see the :ref:`design principles<design-dynamic-sublanguage>`
  section of this documentation. 
- `parametric <https://gitlab.dlr.de/paradigms/parametric>`_: This is the 
  library used to track the data dependencies of the feature tree using a 
  special kind of memoization approach.

The easiest way to install the requirements is via
conan. Make sure you setup 
conan to use the paradigms gitlab package registry, see the 
:ref:`Installation <setup-conan>` section.

 - Create a ``build`` directory and install the dependencies using conan. 
   To install the debug versions of the dependencies, enter the following
   commands:

   .. code-block:: console

      mkdir build && cd build
      conan install .. -r gitlab -s build_type=Debug --build=missing


   This will generate ``Find<XXX>.cmake`` files for you which will be used 
   in the next step.

Building using CMake
====================
  
- Enter the following commands to build grunk 
  in debug mode from source using the generator `ninja`. If you haven't 
  already done so in the :ref:`steps above <prerequisites>`, create 
  a build directory and navigate to it.

  .. code-block:: console 

     mkdir -p build && cd build

- Now you can build grunk using cmake:

  .. code-block:: console

     cmake .. -DCMAKE_BUILD_TYPE=Debug -DGRUNK_TESTS=ON -GNinja
     ninja


Running the unit tests
======================

- Make sure everything works by running the unit tests. 

  - Set the paths to all required shared libraries. The easiest way to do 
    this is using a conan virtual run environment. Assuming you are using 
    Linux, enter:

    .. code-block:: console

       conan install .. --generator=virtualrunenv -s build_type=Debug
       chmod a+x activate_run.sh
       ./activate_run.sh
  
    On Windows you can skip the ``chmod`` step and you would execute 
    ``activate_run.ps`` or ``activate_run.bat`` in the following step.
  
  - Now run the unit tests:

    .. code-block:: console

      cd tests
       ./runUnitTests

Building this documentation
===========================

This documentation is built by default as part of the build process. You can find the 
generated html files in ``build/docs/html/``.