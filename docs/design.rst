.. SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
..
.. SPDX-License-Identifier: MPL-2.0

*****************
Design Principles
*****************

grunk's core functionality is designed around two key concepts :
 * :ref:`dynamic typing<design-dynamic-sublanguage>`
 * :ref:`parametric trees<design-features-actions>`

.. _design-dynamic-sublanguage:

Dynamic Typing
==============

C++ is a statically typed language and all types must be determined at 
compile time. To be able to load user defined feature types at runtime and 
do something with them, grunk works with type-erased objects 
called ``gurnk::object``s as well as function wrappers that accept and 
return ``grunk::objects``\s. Both classes build on the LUA scripting language. 

.. _design-features-actions:

Parametric Trees
================

A ``grunk::Feature`` is a node in a *feature tree*. It is a class template that can 
either wrap an instance of any class or - if dynamic typing is needed - an instance 
of a ``grunk::object``. The latter is the case, if custom types from a plugin shall be used.

``grunk::Feature``\s can be independent - in which case they 
serve as input parameters of your model, or they can be the result of 
``grunk::Action``\s.

An ``grunk::Action`` represents a calcuation step. Just like a ``grunk::Feature``, it is a class
template that can wrap any function . ``Action``\s can be applied 
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