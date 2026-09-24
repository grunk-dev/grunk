.. SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
..
.. SPDX-License-Identifier: MPL-2.0

*****************
Writing Plugins
*****************

Writing a plugin means picking one of the three kinds described in
:ref:`Grunk plugins<using-plugins>` and giving it a name and a version.

.. _writing-plugins:

Plain C++ plugin
=================

A plain C++ plugin is a shared library exporting two fixed entry points, so a generic loader
(``grunk::plugin::load_native``) can find them without already knowing the plugin's name:

.. code-block:: cpp

   // my_plugin.cpp - built as its own shared library, e.g. my_plugin.so
   #include <grunk/grunk.hpp>
   #include <grunk/plugin.hpp>

   struct MyScalar
   {
       double get() const { return v; }
       double v;
   };

   GRUNK_PLUGIN_INFO("my_plugin", "1.0.0")

   void register_my_plugin(grunk::state& state, grunk::PluginInfo const& info)
   {
       // begin_plugin returns a plugin_namespace proxy and records the plugin's
       // identity - the proxy remembers its own namespace table and qualifier, so
       // register_type/register_function calls against it don't need to repeat
       // either one, unlike an ordinary register_type/register_function call
       // against the raw environment in the "Interacting with the Lua state"
       // example above.
       auto ns = state.begin_plugin(info);

       ns.register_type<MyScalar>("MyScalar")
           .add_constructors([](double v) { return MyScalar{v}; })
           .add_member_function("get", &MyScalar::get);

       ns.register_function(
           "add",
           [](MyScalar const& l, MyScalar const& r) { return MyScalar{l.get() + r.get()}; }
       );
   }
   GRUNK_PLUGIN_REGISTER(register_my_plugin)

``GRUNK_PLUGIN_INFO`` defines the ``grunk_plugin_info`` entry point, reporting the plugin's identity;
``register_my_plugin`` (wrapped into the actual ``grunk_plugin_register`` entry point by
``GRUNK_PLUGIN_REGISTER`` - see below) is handed that same ``PluginInfo`` back and does the actual
registration. ``GRUNK_PLUGIN_INFO`` is equivalent to writing
``GRUNK_PLUGIN_EXPORT grunk::PluginInfo grunk_plugin_info() { return grunk::PluginInfo{name, version}; }``
by hand, plus an MSVC-only ``#pragma`` suppressing warning C4190 (an ``extern "C"`` function returning a
non-POD type - ``PluginInfo`` holds two ``std::string`` members - which is expected here, not a bug: see
the note on ``GRUNK_PLUGIN_EXPORT`` below on why the ``extern "C"`` is only about symbol naming, not
real C ABI compatibility). Always prefer the macro over the hand-written form. Registering
against ``ns`` (a
``grunk::plugin_namespace``) instead of the raw environment is what puts ``MyScalar``/``add`` under
the ``my_plugin`` namespace (``my_plugin.MyScalar``, ``my_plugin.add``) instead of flat in the
environment, and keeps their *serialized* form (used when writing a recipe to file) resolvable by
that same qualified path when the recipe is read back in - ``ns`` already knows both its own table
and its qualifier (``info.name``), so there is nothing left to pass explicitly at each call. Code
that genuinely needs the raw ``sol::table`` (e.g. to override the qualifier, or to call
``grunk::state``'s own ``register_type``/``register_function`` directly) can still get at it via
``ns.table()`` (or the implicit conversion ``ns`` itself supports).

``GRUNK_PLUGIN_EXPORT`` (not a plain ``extern "C"``) is what makes this portable: a plain
``extern "C"`` is enough on Linux/macOS, where a shared library exports its symbols by default,
but not on Windows, where a DLL exports nothing unless a symbol is explicitly marked
``__declspec(dllexport)`` - exactly what ``GRUNK_PLUGIN_EXPORT`` expands to there.


`GRUNK_PLUGIN_REGISTER` generates the actual `grunk_plugin_register` entry point around your
registration function: it catches any exception your function throws *on the plugin's own side* of
the `dlopen`/`dlsym` boundary and reports failure as a plain error string instead, since an
uncaught C++ exception is not safe to unwind across that boundary unless the plugin and the host
were built with the exact same compiler, standard library, and grunk/sol2/Lua versions - a real risk
once a plugin is its own separate build, as `my_plugin.cpp` above would typically be. Always use the
macro rather than defining `grunk_plugin_register` by hand.

Registering an overloaded method
-----------------------------------

Wrapping an existing C++ type is where overloaded methods first become a problem: a bare
``&Type::Method`` doesn't compile when ``Method`` is overloaded, since there's no single function
pointer type to deduce - ``add_member_function``'s ``F`` template parameter has nothing to bind
against. Picking one overload takes an explicit member-pointer ``static_cast``:

.. code-block:: cpp

   struct MyScalar
   {
       double get() const { return v; }
       void set(double x) { v = x; }
       void set(double x, double scale) { v = x * scale; }
       double v;
   };

   ns.register_type<MyScalar>("MyScalar")
       .add_constructors([](double v) { return MyScalar{v}; })
       .add_member_function("get", &MyScalar::get)
       .add_member_function("set",
           static_cast<void (MyScalar::*)(double)>(&MyScalar::set));

If more than one overload genuinely needs to be reachable from Lua under the same name,
``add_member_functions`` (plural) takes several such casts at once and wraps them in a single
``sol::overload(...)`` set, mirroring ``add_constructors``'s ergonomics:

.. code-block:: cpp

   ns.register_type<MyScalar>("MyScalar")
       .add_constructors([](double v) { return MyScalar{v}; })
       .add_member_function("get", &MyScalar::get)
       .add_member_functions("set",
           static_cast<void (MyScalar::*)(double)>(&MyScalar::set),
           static_cast<void (MyScalar::*)(double, double)>(&MyScalar::set));

As with a manually-passed ``sol::overload(...)``, a method registered this way gets no return-type
hint, so a ``DynamicFeature`` it returns can't use native colon-call dispatch and needs an explicit
``:as(Type)`` call or the qualified ``TypeName.method(...)`` form instead.

Building the plugin only needs the ``grunk::plugin`` CMake target:

.. code-block:: cmake

   add_library(my_plugin SHARED my_plugin.cpp)
   target_link_libraries(my_plugin PRIVATE grunk::plugin)

Compiled Lua plugin
=====================

A compiled Lua plugin is a shim around an existing compiled Lua C extension you didn't write by
hand - e.g. a `SWIG <https://www.swig.org/>`_ Lua binding for a large existing library. The shim
links directly against that extension and re-exposes it through the same fixed ABI as a plain C++
plugin, so the loader never needs to know the difference:

.. code-block:: cpp

   // my_swig_plugin.cpp
   #include <grunk/grunk.hpp>
   #include <grunk/plugin.hpp>

   // The SWIG-generated module's own entry point, following Lua's luaopen_* convention.
   extern "C" int luaopen_mymodule(lua_State* L);

   GRUNK_PLUGIN_INFO("mymodule", "1.0.0") // name must match the SWIG module's own name

   void register_mymodule_plugin(grunk::state& state, grunk::PluginInfo const& info)
   {
       auto ns = state.load_compiled_plugin(info, luaopen_mymodule);

       // Classes a SWIG-Lua binding exposes need one extra step: their constructor/methods
       // aren't plain table entries the way a free function is, so register_external_type
       // bridges them generically instead of requiring a compile-time C++ type. Unlike
       // state::register_external_type, ns.register_external_type only needs the class's
       // own short name - it auto-prefixes "mymodule." for you.
       sol::table my_class_ctor = ns["MyClass"];
       ns.register_external_type("MyClass", my_class_ctor);
   }
   GRUNK_PLUGIN_REGISTER(register_mymodule_plugin)

``load_compiled_plugin`` loads the module, makes its free functions grunk-trackable, and registers
it under ``info.name`` - unlike ``begin_plugin``, it mints its own namespace table (the module's
own table), so there is no separate namespace to fetch first; ``ns`` is a ``grunk::plugin_namespace``
either way.

Pure Lua plugin
=================

A pure Lua plugin needs no compilation at all - just a Lua source file, loaded with an explicit
name and version (a bare ``.lua`` file carries no identity of its own, unlike a native plugin's
``grunk_plugin_info``):

.. code-block:: lua

   -- my_lua_plugin.lua
   function add_one(x)
       return x + 1
   end

.. code-block:: cpp

   grunk::state grunk;
   grunk.load_lua_plugin_file(grunk::PluginInfo{"my_lua_plugin", "1.0.0"}, "my_lua_plugin.lua");

``load_lua_plugin_file``/``load_lua_plugin_script`` are thin wrappers over
``run_module_file``/``run_module_script`` (see :ref:`Modules<module>`) that additionally record
the plugin's identity, so this kind shares the same ``uses:``-block validation and namespacing as
the other two.

.. _generating-bindings:

Generating bindings with the code generator
==============================================

Every plain C++ plugin above is really just a list of ``register_type``/``add_constructors``/
``add_member_function`` calls - one per class, constructor, and method you want to expose. Writing
that by hand is fine for a small, purpose-built type like ``MyScalar`` above. It's a chore for an
*existing* C++ library, whose header already lists every class and method you'd want - you'd be
typing the same information twice, and the two copies drift apart the moment the library's header
changes.

``grunk.codegen`` (part of the ``grunk`` package, also available as the ``grunk`` command's
``codegen`` subcommand) reads a library's C++ headers and generates that plugin code for you, driven
by a small ``codegen/config.yml`` next to your plugin's own source.

Getting started
-------------------

A minimal ``config.yml`` just lists which headers to read:

.. code-block:: yaml

   modules:
     - name: widgets
       headers:
         - widgets.hpp        # a literal filename
         - shapes_*.hpp       # or a glob pattern, matched against --include-dir

Run it with:

.. code-block:: console

   grunk codegen --config codegen/config.yml --include-dir /path/to/library/include --output-dir generated

For every class, struct, enum, and namespace it finds, this generates code registering every public
constructor and method it knows how to bind. Most of the time, that's everything you'd hope for:
plain numbers and booleans, enums, and classes - including a class from a *different* library or
plugin, since Lua doesn't need to know about a type ahead of time to use it. What it genuinely can't
figure out on its own (a smart pointer, a fixed-size array, or some other library-specific pattern)
gets skipped - never silently, though: every run prints a summary of what it accepted and what it
skipped, and why, so you always know what (if anything) needs a bit of extra help, described below.

A few more config keys cover most of what's left, still plain data, no Python required:

.. list-table::
   :header-rows: 1

   * - Key
     - Purpose
   * - ``modules[].blacklist``
     - A list of ``ClassName``/``ClassName::MethodName`` symbols to exclude outright, each usually
       commented with why.
   * - ``modules[].exclude_headers``
     - Drops specific glob-matched filenames *before* they're parsed - for a header that doesn't
       parse standalone at all.
   * - ``modules[].extra_includes``
     - Extra ``#include`` lines a module's generated ``.cpp`` needs for a type it only forward-declares
       itself.
   * - ``translation_units``
     - Controls whether each module compiles as its own translation unit (the default) or several
       are grouped/merged together - see the generator's own docstring for when this matters.

Teaching it your library's own patterns
-----------------------------------------------------

The generator doesn't know anything about your specific library - only the handful of common C++
patterns described above. For anything your library does differently - a smart-pointer wrapper, a
fixed-size container, a type that looks different depending on a build flag, or any other
library-specific quirk - you teach the generator with a small Python file. Point ``config.yml`` at it
with an optional ``code_generator:`` key, naming a file (relative to ``config.yml`` itself) that
subclasses ``grunk.codegen.generate.CodeGenerator`` and overrides only what it needs, calling
``super()`` for everything else:

.. code-block:: yaml

   code_generator: hooks.py

.. code-block:: python

   # codegen/hooks.py
   from grunk.codegen import generate

   class Hooks(generate.CodeGenerator):
       def classify_param(self, param, registered_types, registered_enums):
           # "widget_lib::vec3" is this library's own fixed-size 3-element
           # container - convert it to/from a plain std::array<double, 3> at
           # the Lua boundary.
           return "vec3" if param.base == "widget_lib::vec3" else None

       def emit_param(self, param, kind):
           if kind != "vec3":
               return None  # not ours - fall back to the plain builtin declaration
           return (f"std::array<double, 3> const& {param.name}",
                   f"widget_lib::vec3({param.name}[0], {param.name}[1], {param.name}[2])")

That's usually all it takes - most plugins need only a couple of overrides like this, not a rewrite
of the generator. Here's what you can override:

.. list-table::
   :header-rows: 1
   :widths: 28 42 30

   * - Method
     - What it's for
     - Example
   * - ``classify_param(param, registered_types, registered_enums)``
     - Recognizing a parameter/return type the built-ins don't already handle - return any name you
       like for it. Returning ``None`` doesn't necessarily mean "unsupported" - a plain class name is
       usually accepted anyway, with no help from you (see "Passing a type through unchanged" below).
     - ``"vec3"`` for a ``widget_lib::vec3`` parameter, above.
   * - ``classify_ctor_mode(class_name, base_chain)``
     - Deciding how a class gets constructed, beyond the two defaults (a plain value, or a
       ``unique_ptr`` for a class with a destructor). ``base_chain`` lists that class's parent
       classes - inspect it for a base class name or naming convention that signals something
       different is needed.
     - ``"handle"`` if ``"RcBase"`` appears anywhere in ``base_chain`` - see "A worked example"
       below.
   * - ``emit_param(param, kind)``
     - The declaration and call text for one parameter of a kind your own ``classify_param``
       returned.
     - ``("std::array<double, 3> const& x", "widget_lib::vec3(x[0], x[1], x[2])")`` for ``"vec3"``,
       above.
   * - ``emit_constructor(class_name, ctor_mode, param_decls, args_str)``
     - The full constructor expression, for any ``ctor_mode`` beyond the two defaults.
     - Heap-allocate and wrap in a smart pointer instead of returning by value - see "A worked
       example" below.
   * - ``emit_bases(class_name, chain)``
     - Which of a class's parent classes to actually register (``chain`` lists all of them). Default:
       all of them. Override to leave some out, e.g. when the deeper ones aren't meaningful on the
       Lua side.
     - Stop right after a named base class - see "A worked example" below.
   * - ``emit_return_type(return_spelling)``
     - Rewriting the emitted C++ return-type text, if your library needs it (e.g. dropping a
       reference off a returned smart-pointer type).
     - Strip a ``&`` off a returned ``widget_lib::Rc<T> const&`` so it's returned by value instead.
   * - ``emit_callable(callable, kinds, ctor_mode)``
     - The one method that builds everything else: given a constructor, method, or function and its
       already-classified parameters, it returns the full C++ expression for it. Override this
       *wholesale* (calling it again via ``super()``) when one callable needs more than one version
       of itself - see below.
     - See "Supporting build-time variants" below.

Supporting build-time variants
---------------------------------------------------------------

Some libraries can be built two different ways - say, with or without support for a third-party
algorithmic-differentiation library - where a handful of parameter types need different C++ text
depending on which build is active, selected by a preprocessor macro. The generator has no built-in
idea of "variants" at all - you get the same effect just by overriding ``emit_callable`` and calling
it twice, once per variant, and combining the two results yourself:

.. code-block:: python

   class Hooks(generate.CodeGenerator):
       _ad_variant = False  # private state - only this file ever reads it

       def classify_param(self, param, registered_types, registered_enums):
           # "Scalar" is this library's own typedef for a real number, the
           # same role OCCT's Standard_Real or ADOL-C's adouble play for
           # their own libraries. A plain C++ "double" is already handled by
           # the generator's own built-in support and never reaches this
           # method at all - which is exactly why a library needing two
           # variants gives its scalar a named typedef instead of "double".
           return "scalar" if param.base == "Scalar" else None

       def emit_param(self, param, kind):
           if kind == "scalar" and self._ad_variant:
               # AD-enabled build: declare the parameter as the AD library's
               # own scalar type instead - no conversion needed, just a
               # different declared C++ type.
               return f"adouble {param.name}", param.name
           return None  # plain build: "Scalar" needs no special handling at all

       def emit_callable(self, c, kinds, ctor_mode):
           if "scalar" not in kinds:
               return super().emit_callable(c, kinds, ctor_mode)
           plain = super().emit_callable(c, kinds, ctor_mode)
           self._ad_variant = True
           try:
               alternate = super().emit_callable(c, kinds, ctor_mode)
           finally:
               self._ad_variant = False
           return f"\n#if defined(MY_LIB_WITH_AD)\n{alternate}\n#else\n{plain}\n#endif\n"

Calling ``super().emit_callable(...)`` twice - once as normal, once with a private flag flipped that
only this file's own ``emit_param`` reads back - is what produces the two variants. The generator
itself never needs to know anything about "variants" or your library's own name; it's an ordinary
Python trick, entirely owned by your plugin's own ``hooks.py``. See
``python/grunk/codegen/README.md`` in the grunk repository if you want to go deeper into how the
generator itself works.

Passing a type through unchanged
---------------------------------------------------

You don't always need to understand a type to accept it. Sometimes you just want to hand it off to
some other call untouched - a callback, an opaque handle, anything your plugin only forwards along
without inspecting. For that, declare the parameter as a plain ``sol::object`` in ``emit_param``:

.. code-block:: python

   def classify_param(self, param, registered_types, registered_enums):
       return "callback" if param.base == "widget_lib::Callback" else None

   def emit_param(self, param, kind):
       if kind != "callback":
           return None  # not ours - some other kind (or a built-in) handles this parameter
       return f"sol::object {param.name}", param.name

You'll rarely need this for an ordinary class from your own library, or from another plugin -
those are usually accepted automatically, as described under "Getting started" above. It's mainly
for a type you're deliberately choosing not to look inside.

A worked example: reference-counted classes
----------------------------------------------------------

Some libraries always heap-allocate a class and hand it back wrapped in a reference-counted smart
pointer, rather than letting you construct it directly. Here's how to teach the generator that
pattern, in a few lines of ``hooks.py``:

.. code-block:: python

   class Hooks(generate.CodeGenerator):
       def classify_ctor_mode(self, class_name, base_chain):
           return "handle" if "RcBase" in base_chain else None

       def emit_bases(self, class_name, chain):
           # Stop the base-class list right at RcBase - anything further up
           # is an internal detail, not something Lua needs to see.
           if "RcBase" in chain:
               return chain[: chain.index("RcBase") + 1]
           return chain

       def emit_constructor(self, class_name, ctor_mode, param_decls, args_str):
           # Only called for ctor_mode == "handle" above - every other class
           # still gets constructed the normal way.
           return (f"[]({', '.join(param_decls)}) -> widget_lib::Rc<{class_name}> "
                   f"{{ return widget_lib::Rc<{class_name}>(new {class_name}({args_str})); }}")

The generator itself has no idea what "``RcBase``" means, or why its presence should change
anything - that decision lives entirely in this small, self-contained file, not in the shared tool.
See ``grunk-occt``'s own ``codegen/hooks.py`` in the `grunk-occt repository
<https://github.com/grunk-dev/grunk-occt>`_ for a real, more elaborate version of this pattern.

.. _sharing-plugins:

Sharing Plugins
==================

Not yet implemented. Today, sharing a grunk plugin means sharing its source and build
instructions. A package manager for distributing built grunk plugins, so a consumer
doesn't need a C++ toolchain at all, is planned future work.
