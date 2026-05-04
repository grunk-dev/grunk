.. SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
..
.. SPDX-License-Identifier: MPL-2.0

=================
Design Principles
=================

grunk's core functionality is designed around two pairs of classes:
 * :ref:`DynamicObjects and DynamicFunctions<design-dynamic-sublanguage>`
 * :ref:`Features and Actions<design-features-actions>`

.. _design-dynamic-sublanguage:

DynamicObjects and DynamicFunctions
===================================

C++ is a statically typed language and all types must be determined at 
compile time. To be able to load user defined feature types at runtime and 
do something with them, grunk works with type-erased objects 
called ``DynamicObject`` as well as ``DynamicFunction``\s that accept and 
return ``DynamicObject``\s. In a way, ``DynamicObject``\s and 
``DynamicFunction``\s provide a minimalistic dynamically typed sublanguage
in C++. 

Basically, a ``DynamicObject`` is an 
`std::any <https://en.cppreference.com/w/cpp/utility/any>`_ on steroids: 
In addition to the type-erased object, it stores a type descriptor with 
some functionality for accessing constructors as well as data members and 
member functions. This type descriptor must be created once by *reflecting* 
the actual C++ type. This is done at compile time of a plugin, if it  
provides the corresponding feature type as part of its interface. 

A ``DynamicFunction`` wraps any kind of normal function, but it accepts and returns 
``DynamicObject``\s. The types in the signature of the function must have 
previously been *reflected*. 

If a function returns a ``std::tuple``, the
corresponding ``DynamicFunction`` will return a vector of
``DynamicObject``\s. This facilitates the use of multi-output functions in 
the dynamic typing system.

For its dynamic typing system, grunk uses the 
`reflect <https://gitlab.dlr.de/paradigms/reflect>`_ library.

Why not use a dynamically typed scripting language like python from the 
start? Firstly, python would be a big dependency for grunk and would hinder 
the integrability. Most features of Python are not needed for grunk. It 
should be easy to use grunk as a fairly light-weight C++ library. Secondly, 
both the ``DynamicObject``\s and python's ability to be dynamically typed 
come at a performance overhead because types must be resolved at runtime 
via type-erasure and runtime polymorphism techniques. Using a minimalistic 
sublanguage in C++ gives us fine-grained control over just how much dynamic 
typing we need.

.. _design-features-actions:

Features and Actions
=======================

To understand how grunks dynamic typing system works, it is useful to know 
about `DynamicObject``\s and ``DynamicFunction``\s.

As a user of grunk however, you will work with ``Feature``\s and ``Action``\s 
rather than ``DynamicObject``\s and ``DynamicFunction``\s.

A ``Feature`` is a node in a *feature tree*. It is a class template that can 
either wrap an instance of any class or - if dynamic typing is needed - an instance 
of a ``DynamicObject``. The latter is the case, if custom types from a plugin shall be used.

``Feature``\s can be independent - in which case they 
serve as input parameters of your model, or they can be the result of 
``Action``\s.

An ``Action`` represents a calcuation step. Just like a ``Feature``, it is a class
template that can either wrap any function or - if dynamic typing is needed - a 
``DynamicFunction``. ``Action``\s can be applied 
to ``Feature``\s just like a functions can be applied to its inputs. The 
main difference is, that when the ``Action`` is applied, no calculation 
is performed. Instead, the data dependencies between the ``Action``\s 
input ``Feature``\s and output ``Feature``\s are registered together with 
the actual computation that the action performs. This computation is 
delayed until the value of one of the ``Action``\s output ``Feature`` 
gets explicitly queried. Then, the calculation result is cached until in the 
``Feature`` gets invalidated. This happens, once one of its dependent 
``Feature``\s down the feature tree gets invalidated, e.g. by a change of a 
root ``Feature``.

The DAG management of ``Feature``\s and ``Action``\s uses the 
`parametric <https://gitlab.dlr.de/paradigms/parametric>`_ library.