=================
Design Principles
=================

grunk's core functionality is designed around two pairs of classes:
 * :ref:`RuntimeObjects and RuntimeFunctions<design-dynamic-sublanguage>`
 * :ref:`Features and Algorithms<design-features-algorithms>`

.. _design-dynamic-sublanguage:

RuntimeObjects and RuntimeFunctions
=====================================

C++ is a statically typed language and all types must be determined at 
compile time. To be able to load user defined feature types at runtime and 
do something with them, grunk works with type-erased objects 
called ``RuntimeObject`` as well as ``RuntimeFunction``\s that accept and 
return ``RuntimeObject``\s. In a way, ``RuntimeObject``\s and 
``RuntimeFunction``\s provide a minimalistic dynamically typed sublanguage
in C++. 

Basically, a ``RuntimeObject`` is an 
`std::any <https://en.cppreference.com/w/cpp/utility/any>`_ on steroids: 
In addition to the type-erased object, it stores a type descriptor with 
some functionality for accessing constructors as well as data members and 
member functions. This type descriptor must be created once by *reflecting* 
the actual C++ type. This is done at compile time of the plugin that 
provides the correspong feature type. For this, grunk uses the 
`reflect <https://gitlab.dlr.de/paradigms/reflect>`_ library.

A ``RuntimeFunction`` creates a function accepting and returning 
``RuntimeObject``\s from any function accepting and returning types, which 
have previously been *reflected*. 

Why not use a dynamically typed scripting language like python from the 
start? Firstly, python would be a big dependency for grunk and would hinder 
the integrability. Most features of Python are not needed for grunk. It 
should be easy to use grunk as a fairly light-weight C++ library. Secondly, 
both the ``RuntimeObject``\s and python's ability to be dynamically typed 
come at a performance overhead because types must be resolved at runtime 
via type-erasure and runtime polymorphism techniques. Using a minimalistic 
sublanguage in C++ gives us fine-grained control over just how much dynamic 
typing we need.

.. _design-features-algorithms:

Features and Algorithms
=======================

As a user of grunk, you will work with ``Feature``\s and ``Algorithm``\s 
rather than ``RuntimeObject``\s and ``RuntimeFunction``\s. 

A ``Feature`` is a node in a *feature tree*, which wraps an instance of a 
``RuntimeObject``. ``Feature``\s can be independent - in which case they 
serve as input parameters of your model, or they can be the result of 
``Algorithm``\s.

An ``Algorithm`` wraps a ``RuntimeFunction``. ``Algorithm``\s can be applied 
to ``Feature``\s just like a functions can be applied to its inputs. The 
main difference is, that when the ``Algorithm`` is applied, no calculation 
is performed. Instead, the data dependencies between the ``Algorithm``\s 
input ``Feature``\s and output ``Feature``\s are registered together with 
the actual computation that the algorithm performs. This computation is 
delayed until the value of one of the ``Algorithm``\s output ``Feature`` 
gets explicitly queried. Then, the calculation result is cached until in the 
``Feature`` gets invalidated. This happens, once one of its dependent 
``Feature``\s down the feature tree gets invalidated, e.g. by a change of a 
root ``Feature``.

The DAG management of ``Feature``\s and ``Algorithm``\s uses the 
`parametric <https://gitlab.dlr.de/paradigms/parametric>`_ library.