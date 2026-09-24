<!--
SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>

SPDX-License-Identifier: MPL-2.0
-->

# grunk.codegen

A libclang-based generator that parses a set of C++ headers (driven by a
per-plugin `codegen/config.yml`) and emits grunk plugin registration code
targeting `register_type`/`add_constructors`/`add_member_function`
(`generated/<module>.{hpp,cpp}` - conventionally checked into the consuming
plugin's own repo, see its own `DESIGN.md`'s "Generated-code policy").

**This module has no knowledge of, or reference to, any specific third-party
library.** Everything it understands out of the box - plain value construction,
heap-allocation via `std::unique_ptr` for a class with an explicit destructor,
passthrough of a registered class/enum/C++ fundamental - is genuinely universal
C++. A plugin whose library uses anything beyond that (a reference-counted
smart-pointer wrapper, a fixed-size array/container type, a type with more than
one possible C++ representation selected by a build-time macro, or any other
library-specific idiom) teaches the generator via an optional
`code_generator:` key in its own `config.yml`, naming a Python file that
subclasses `CodeGenerator` (see that class's own docstring in `generate.py`) and
overrides only what it needs.

See the project's Sphinx documentation, "Writing Plugins" -> "Generating
bindings with the code generator", for a full tutorial with a worked example.
This file stays a short reference to the module's own structure; for a
*concrete*, real-world `CodeGenerator` subclass and the design rationale behind
each of its overrides, see a consuming plugin's own `codegen/README.md` (e.g.
`grunk-occt`'s, which documents its own `codegen/hooks.py` in depth).

## Running it

```
grunk codegen --config codegen/config.yml --include-dir /path/to/library/include --output-dir generated
```

Review the stderr summary (accepted/rejected/blacklisted counts, and every
rejection's reason) after every run - it's the primary way to notice a symbol
that needs a hand-written escape hatch, or a `CodeGenerator` override.

## Structure

- **`Param`/`Callable`/`Entity`**: the parsed-AST model. `Param.kind()` returns
  `"passthrough"`/`"object"` for the built-in fundamental/enum/registered-class cases, delegates to
  `CodeGenerator.classify_param()` for anything else, and - only if that also returns `None` - falls
  back to classifying any remaining class-shaped name as `"object"` regardless of whether this run
  itself registers it (see its own docstring for the exact conditions, including why a mutable
  reference needs its canonical spelling to positively confirm it isn't a fundamental hiding behind
  an unfamiliar typedef). `classify_param()` returning `None` therefore does not mean a parameter is
  rejected outright - only that this hook didn't recognize it.
- **`discover_entities()`/`collect_class_callables()`/`collect_namespace_callables()`/
  `collect_friend_functions()`**: the libclang-facing parsing layer - finds every
  class/struct/enum/namespace directly declared in a header, and every public
  constructor/method/operator/friend-function on a class. Deliberately narrow in
  scope (constructors/instance methods only) and entirely generic - no
  library-specific logic anywhere in this layer.
- **`process_module()`**: decides, per discovered entity, what's accepted
  (`Callable.accepted_arities()`, including default-argument expansion),
  rejected (with a reason), or blacklisted; decides each class's constructor
  mode (`"value"`/`"unique_ptr"` built in, or `CodeGenerator.classify_ctor_mode()`
  for anything else).
- **`CodeGenerator`**: the one extension point - see its own docstring for the
  full method list (`classify_param`, `classify_ctor_mode`, `emit_param`,
  `emit_constructor`, `emit_return_type`, `emit_callable`) and what each is for.
  `load_code_generator()` loads a plugin's own subclass from the path named by
  `config.yml`'s `code_generator:` key (absent -> the base class itself, unmodified
  - the trivial case, needing zero hook code).
- **`render_module()`**: turns one module's accepted callables into the actual
  `.hpp`/`.cpp` text, including the module's own `helper_namespace`-free
  `includes_for_kinds`/`includes_for_ctor_modes` config lookups for any extra
  `#include`s a plugin's own kinds/ctor-modes need.

## Config.yml keys this module reads

Purely declarative, no library-specific meaning in any of these:

- `modules[].name`/`headers` (literal filenames and/or glob patterns, expanded
  against `--include-dir`)/`blacklist`/`exclude_headers`/`extra_includes`.
- `translation_units.mode`/`.groups` (see `compute_translation_units()`'s own
  docstring for when a `"handle"`-style ctor mode requires this).
- `code_generator` (optional, a path to a `CodeGenerator` subclass - see above).
- `includes_for_kinds`/`includes_for_ctor_modes` (optional; keyed by whatever
  kind/ctor-mode names a plugin's own `CodeGenerator` subclass returns).
- `generated_banner` (optional, a verbatim multi-line string - a YAML `|` block
  scalar is the natural way to write one - written at the top of every emitted
  `.hpp`/`.cpp`, typically an SPDX header plus an AUTO-GENERATED notice; see
  `DEFAULT_GENERATED_BANNER`'s own comment for the generic fallback used when
  absent, and why this module doesn't assume any plugin's copyright/license by
  default).

## Known gaps

- Toolchain include-path discovery (`system_cxx_include_dirs()`) is GCC/Clang-only
  (probes `$CXX`/`c++` with `-E -x c++ -v -`) - doesn't work for MSVC. Not a
  blocker today since codegen is opt-in and its output is conventionally checked
  into git (regeneration only needs to happen on whatever platform a maintainer
  actually runs it from).
- One clang parse per header (no shared compilation database/precompiled
  headers) - the `ParsedHeader` cache already avoids re-parsing the same header
  twice across one run; sharing a compilation database across *different*
  headers remains a possible further optimization if generation time ever
  becomes a real bottleneck.
- Multiple inheritance is not handled in the base-chain walk - only the first
  base specifier per class is followed.
- A mutable-reference parameter/return type not otherwise recognized (not a
  fundamental/enum, not one of this run's own `registered_types`, not
  classified by a hook) is only accepted when its own *canonical*
  (typedef-resolved) spelling positively confirms it's a genuine class - see
  `Param.kind()`'s own docstring. There's no canonical spelling available at
  all for a *return* type (only ever built as a synthetic, name-only `Param`
  by `Callable.rejection_reason()`/`accepted_arities()`), so a mutable
  reference returned by value always stays conservative and rejected, even
  when it genuinely is a class. Fixable in principle (give the synthetic
  return-type `Param` its own canonical spelling too, from
  `child.result_type.get_canonical().spelling`, the same way a real
  parameter's `canonical` is already populated), just not done yet - nothing
  has needed it badly enough to justify the extra libclang plumbing.
- A smart-pointer/handle-style wrapper (e.g. OCCT's own
  `opencascade::handle<T>`) around a type registered by a *different* plugin
  is a real, open gap, not just a missing `classify_param` teaching - see
  `grunk-occt/DESIGN.md`'s "Cross-plugin Handle(X) sharing" for the concrete
  constraint and candidate directions.
