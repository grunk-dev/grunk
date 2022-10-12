.. `toctree`

.. _grunk:

*************
C++ API
*************

This section contains a documentation of the C++ classes and functions. All 
classes and functions in the documentation reside in the namespace ``grunk``. 
To use grunk's C++ API, you have to include the header `grunk.h`.

grunk supplies two modes of operation, that will be denoted as 
:ref:`static mode <static-mode>` and :ref:`dynamic mode <dynamic-mode>`. Use the static mode of grunk, if you know all the types you want to use at 
compile time. Use the dynamic mode of grunk, if you want to use types and functions provided
by plugins which are loaded at run time. In this case, grunk falls back to its
dynamic type system, see also the :ref:`design principles <design-dynamic-sublanguage>`
section of this documentation.

.. _static-mode:

Static Mode
===========

The core of grunk consists of the ``eval`` function together with 
``Feature`` class.

A ``Feature`` represents any kind of feature in the feature tree.
Given any kind of *referentially transparent* function and some input features
from the feature tree, the function ``eval`` registeres the evaluation
of the given function for the given features as input in the feature tree.

.. code-block:: cpp

   grunk::Feature<double> l("l", 3.3);
   grunk::Feature<double> r("r", 2.2);
   auto res = grunk::eval("o", std::add, l, r);



The output features can be queried from the returned `AlgorithmPtr` instance, see
also the :ref:`advanced section <advanced>`.

.. code-block:: cpp

   auto o = res->get(); // retrieve the first (and in this case only) output of the calcuation
   std::cout<<o.value()<<std::endl; // evaluate the result, thus triggering the calculation

.. doxygengroup:: static
   :content-only:
   :members:


.. _dynamic-mode:

Dynamic Mode
============

Just like in static mode, the usage of grunk revolves around ``RuntimeFeature``\s
and an overload of the ``eval`` function.

The main difference is that this class and function now are called with the string 
representation of types and functions that do not need to be known at compile time.

.. code-block:: cpp

   grunk::Feature l("l", "double", 3.3);
   grunk::Feature r("r", "double", 2.2);
   auto res = grunk::eval("o", "add", l, r);
   auto o = res->get(); // retrieve the first (and in this case only) output of the calcuation
   std::cout<<Reflect::cast<double>(r.value())<<std::endl; // evaluate the result, thus triggering the calculation

The types and functions must be registered from plugins loaded at run time, see also 
the :ref:`plugin section <plugin-system>` of this documentation or the 
:ref:`corresponding section<using-plugins>` under Usage.

.. doxygengroup:: dynamic
   :content-only:
   :members:

.. _file-io:

File I/O
========

This section documents all functions and classes of grunk's file i/o. This includes
all functionality to reading and writing a feature tree from/to a yaml file.

.. doxygengroup:: fileio
   :content-only:
   :members:

.. _plugin-system:

Plugin system
=============

This section documents all functions and classes of grunk's plugin system. This
includes the ``PluginRegistry`` that manages all registered plugins, the plugin 
interface ``IPlugin``, that plugin authors must use as base class when writing plugins,
and the ``StdPlugin``, that registers standard C++ types and functions.

.. doxygengroup:: plugin
   :content-only:
   :members:


.. _advanced:

Advanced
========

This section contains documentation for the most important internal classes and functions.
This section of the documentation is targeted for developers who want to modify grunk. As 
a user of grunk, you should not need to worry about any of these.

.. doxygengroup:: advanced
   :content-only:
   :members:

.. doxygengroup:: dynamic_advanced
   :content-only:
   :members:
   

   
   
