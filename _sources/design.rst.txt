.. SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
..
.. SPDX-License-Identifier: MPL-2.0

*****************
Design Principles
*****************

grunk's core functionality is organised around two central concepts:

* :ref:`dynamic typing<design-dynamic-sublanguage>`
* :ref:`parametric trees<design-features-actions>`

This chapter explains the rationale behind these concepts and summarizes
important invariants and guarantees that grunk provides. It is intended for
developers who want to understand the runtime model, contribute plugins, or
embed grunk into larger C++ or Python applications.

.. _design-dynamic-sublanguage:

Dynamic Typing
==============

C++ is statically typed: normally, all types must be known at compile time.
To allow loading user-defined types and functions at runtime (for example via
plugins), grunk provides a small dynamic sublanguage based on a type-erased
object and a runtime function registry.

Key points
----------

- grunk exposes a type-erased wrapper called ``grunk::object``. A
	``grunk::object`` can hold any registered user type and can be manipulated
	from C++, Lua, or via the Python bindings.
- Functions and member functions can be registered at runtime and are
	accessible through a string-based lookup in the function registry.
- The dynamic layer is implemented on top of Lua (via the `sol2`
	library). Lua provides a convenient, portable embedding API and a
	lightweight scripting environment for plugin-provided functionality.

Why this design?
----------------

Using type-erasure and a small runtime reflection layer lets grunk load
domain-specific types (for instance CAD objects or physics types) from
shared libraries without requiring those types to be available at compile
time. This makes grunk extensible: new plugins can introduce types,
serialization routines, and functions that participate in parametric trees.

Plugins and serialization
-------------------------

Plugins register types and functions with the grunk runtime. For types that
shall be used as root parameters of recipes or written to disk, the plugin
must also provide simple (de-)serialization hooks so grunk can store and
reconstruct instances. See the plugin authoring section for details.

.. _design-features-actions:

Parametric Trees
================

A ``grunk::Feature`` is the basic node in a feature tree (a directed acyclic
graph). The template ``Feature<T>`` may either wrap a concrete C++ type
(``T``) or — in the dynamic case — a ``grunk::object`` that holds a
plugin-provided instance.

Features
--------

- Independent features serve as inputs to a computation and typically have a
	user-provided value (they are the leaves/roots depending on viewpoint).
- Computed features are the outputs of actions and carry dependency
	information that points to their input features.

Actions and lazy evaluation
---------------------------

An ``grunk::Action`` represents a single computation step. Conceptually it
wraps a function together with the features that should be passed to that
function. Applying an ``Action`` does not execute the function immediately.
Instead it records the dependency edges in the feature graph and produces a
feature that will obtain its value only when requested.

This design enables lazy evaluation and caching: when a feature's value is
queried, grunk walks the minimal subgraph required to produce that value,
evaluates the necessary actions, and stores the result in the feature's
cache. Subsequent queries return the cached value until an input feature
changes and invalidates dependent caches.

Caching, invalidation, and guarantees
-------------------------------------

- Cached results are stored per-feature. grunk guarantees that a cached value
	is returned for repeated queries as long as none of the feature's
	dependencies have changed.
- Invalidation is transitive: changing an independent input invalidates all
	features that (transitively) depend on it. grunk will recompute only the
	invalidated parts of the graph on the next query.

Referential transparency
------------------------

To preserve a well-defined dependency graph and enable safe caching and
parallel evaluation, functions used in actions must be referentially
transparent: they must not mutate their inputs or rely on hidden mutable
global state. Practically, grunk checks (where possible) that functions are
invokable using const references and documents this requirement for plugin
authors.

Parallel execution and static vs dynamic mode
---------------------------------------------

Because the static mode uses native C++ functions and types, it allows
parallel evaluation of independent subgraphs (via ``ParallelExecutor``).
Dynamic mode (Lua and plugin-driven) is single-threaded due to Lua's
execution model and therefore does not support parallel execution.

Implementation note
-------------------

The DAG management and task orchestration uses the
`parametric <https://gitlab.dlr.de/paradigms/parametric>`_ library which
provides the underlying data structures and algorithms for dependency
tracking and evaluation scheduling.