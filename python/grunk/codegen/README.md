<!--
SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>

SPDX-License-Identifier: MPL-2.0
-->

# codegen

A libclang-based generator that parses OCCT headers (driven by `config.yml`) and emits
grunk plugin registration code targeting `register_type`/`add_constructors`/
`add_member_function` (`generated/<module>.{hpp,cpp}`, checked into git - see
`../DESIGN.md`'s "Generated-code policy"). Successor to legacy grocc's
`occt_generator.py`, rebuilt from scratch against the new API.

## Running it

```
pixi run -e default generate_code   # requires -D GRUNK_OCCT_GENERATE_CODE=ON at configure time
```

or directly:

```
pixi run -e default python codegen/generate.py --occt-include-dir .pixi/envs/default/include/opencascade --output-dir generated
```

Review the stderr summary (accepted/rejected/blacklisted counts, and every rejection's
reason) after every run - it's the primary way to notice a symbol that needs a
hand-written escape hatch, or a blacklist entry that's no longer warranted.

## What it does

(see "Lifting the one-class-per-header assumption" / "Wildcard header patterns" /
"Enum support" below for how the scope below is actually discovered)

- **Public constructors, instance methods, and a specific set of operators/friend
  functions** (see "Operator overloads and friend free functions" below - added
  for grunk-adolc's own native ADOL-C binding, not needed by any OCCT module so
  far, but generic, not adouble-specific).
- A parameter/return type is accepted only if it is:
  - `Standard_Real` **by value or const-reference** (gets the
    `#ifdef GRUNK_OCCT_WITH_ADOLC`-gated dual-emission form - see `../DESIGN.md`'s
    "Standard_Real / AD-portability rule"),
  - `Standard_Integer`/`Standard_Boolean` **by value or const-reference** (passed
    through as-is, no AD concern - these aren't the type `adOCCT` retargets),
  - a **plain C-style enum** *also being registered in the same generator run*
    **by value or const-reference** (see "Enum support" below) - passed through
    as-is, same as `Standard_Integer`/`Standard_Boolean`,
  - a **value, const-reference, or non-const-reference** of another class or
    `Handle(X)` *also being registered in the same generator run* (i.e. discovered
    from one of the config's own headers, across every module - see "Type
    registration spans the whole config" below; a type from a module that's
    genuinely never wrapped is rejected, not silently miscompiled),
  - `NCollection_Array1<X>`/`NCollection_Array2<X>` **by value or const-reference,
    parameters only** (see "NCollection_Array1/2 support" below), or
  - `void` (return type only).
- **Non-const-reference ("out") parameters**: accepted, but only when the mutated
  type is itself a registered object/`Handle` type (e.g. `Geom_Curve::D0(Standard_Real
  U, gp_Pnt& P)`) - **not** a corner case to work around, this is a common, idiomatic
  OCCT pattern (`D0`/`D1`/`D2`/`D3`, `Coord`'s 3-out-param overload, ...) and Lua/sol2
  handles it natively: the existing userdata passed in gets mutated in place, exactly
  like OOP mutation already works in Lua. What's **not** supported is a non-const
  reference to a *primitive* (`Standard_Real&`/`Standard_Integer&`/`Standard_Boolean&`)
  - Lua has no mutable-number-by-reference concept, there is no userdata to mutate for
    a bare number (e.g. `gp_Trsf::Transforms(Standard_Real&, gp_XYZ&)` stays rejected
    on the *first* parameter alone, independent of `gp_XYZ` also being unregistered).
  Consuming a mutate-through-reference method from a **recipe** (as opposed to the
  raw/undecorated `create_env()` this project's tests use so far) is where grunk's own
  "module" mechanism becomes relevant - see `../DESIGN.md`'s note.
- Anything else is **rejected** (recorded with a reason, never silently dropped):
  unregistered class types (`gp_XYZ`, `gp_Ax2`, `gp_Quaternion`, `gp_Mat`,
  `gp_TrsfForm`, ...), `Standard_OStream&`/`Standard_SStream`, and any operator not
  covered by "Operator overloads and friend free functions" below.
- **Constructor emission mode is one of three, decided per class** (see
  `../DESIGN.md`): `"value"` (plain `return T(args...);` - the default, used for
  simple value types like `gp_Pnt`/`TopoDS_Shape`), `"handle"` (heap-allocate +
  `opencascade::handle<T>`-wrap, for `Standard_Transient`-derived classes - see
  below), or `"unique_ptr"` (`return std::make_unique<T>(args...);`, for any class -
  or any class in its ancestor chain - that declares an **explicit destructor**
  without `"handle"` mode applying: found the hard way when `BRepAlgoAPI_Cut`
  segfaulted at program exit under the naive `"value"` emission - see `../DESIGN.md`'s
  "Copying an algorithm/builder class segfaults"). This is a conservative heuristic,
  not a proof of unsafety either way - a class with no explicit destructor anywhere
  in its chain is assumed safely copyable.
- **Overloaded methods** (same Lua name, 2+ accepted C++ overloads - e.g.
  `gp_Trsf::SetTranslation`) are merged into one `sol::overload(...)` call
  automatically. Every emitted callable is a **lambda**, never a bare
  `&Type::Method` - this sidesteps the overload-address-ambiguity gotcha filed as
  [grunk-dev/grunk#280](https://github.com/grunk-dev/grunk/issues/280) entirely,
  since calling an overloaded method with concrete argument types is never
  ambiguous, only *taking its address* is.
- **Overload registration order is not arbitrary** - `ambiguity_safe_overload_order()`
  sorts each overload set (constructors, instance methods, static methods/free
  functions) by `(arity, count of "real" kinds)` before emission, rather than
  leaving them in libclang's own cursor-walk (= OCCT header declaration) order.
  Found the hard way (see DESIGN.md's "sol2 overload-resolution ambiguity under
  GRUNK_OCCT_WITH_ADOLC"): under the AD build, every `Standard_Real` parameter
  becomes `sol::object` (needed for the AD dual-emission pattern below), which
  also matches *any* Lua value - so an all-real overload of some arity can
  silently shadow a same-arity, genuinely more specific one (e.g. a `gp_Ax2`-
  taking constructor) if it happens to be registered first, since sol2 commits to
  the first candidate whose parameter count and type checks succeed, and a
  `sol::object` parameter's check never fails. This is exactly what broke
  `examples/tank_model.grr.yml` under AD for an entire migration step before
  being root-caused - not an adOCCT numerical issue, a pure binding-generation
  bug.
- **`Standard_Transient`/`Handle(T)` support**: a class whose ancestor chain
  includes `Standard_Transient` gets its constructors emitted as heap-allocating
  factories returning `opencascade::handle<T>` (`new T(args...)`, never a by-value
  `T`) instead of plain value constructors, and its full ancestor chain emitted as
  `.add_bases<Base1, Base2, ...>()` (needed for Lua colon-call dispatch to reach a
  method registered on a *base* class, and for virtual dispatch through it to reach a
  derived override - see `codegen/config.yml`'s `geom` module and
  `tests/test_geom.cpp`). `Handle(X)` (i.e. `opencascade::handle<X>`) parameters/
  returns are accepted wherever `X` is itself a registered type, bridged via
  `../src/occt_sol_traits.hpp`'s `sol::unique_usertype_traits` specialization
  (included automatically when a module has any Transient-derived class).
  **Multiple inheritance is not handled** - only the first `CXX_BASE_SPECIFIER` per
  class is followed; fine for every OCCT class this generator has targeted so far
  (single inheritance throughout), but would need extending if that changes.
- **`NCollection_Array1<X>`/`NCollection_Array2<X>` support**: a
  by-value or const-reference parameter whose type is (or is a typedef for) one of
  these templates - e.g. `TColgp_Array1OfPnt` (`NCollection_Array1<gp_Pnt>`),
  `TColStd_Array1OfReal` (`NCollection_Array1<double>`) - is accepted whenever `X`
  is itself a supported element type (a registered class, or `double`/`float`/
  `Standard_Integer`/`Standard_Boolean`), and rendered as `std::vector<X> const&`
  (array1) or `std::vector<X> const&` plus two extra `Standard_Integer rows, cols`
  parameters (array2 - see `../DESIGN.md`'s "NCollection_Array2 Lua-side shape" for
  why flat + dims, not nested vectors), converted at the call site via
  `../src/collections.hpp`'s `to_array1`/`to_array2`. Detecting this requires
  libclang's *canonical* (typedef-resolved) type - `TColgp_Array1OfPnt`'s own
  spelling never mentions `NCollection_Array1` at all - which is why `Param` also
  carries a `canonical` field (`Param.kind()`'s `ARRAY1_PATTERN`/`ARRAY2_PATTERN`
  match against it, not the ordinary `spelling`). This is what lets e.g.
  `Geom_BezierCurve`'s own constructors (`(TColgp_Array1OfPnt const&)` and
  `(TColgp_Array1OfPnt const&, TColStd_Array1OfReal const&)`) auto-generate as
  ordinary `add_constructors(...)` overloads, with no hand-written escape hatch.
  A "real" array element's own *Lua-side* vector is always `std::vector<double>`
  regardless of build (AD dual-emission is not extended to the array *element
  type* itself, only to whether the array's own OCCT element type is
  `Standard_Real`) - but a real-element `array1`/`array2` parameter is converted
  via `../src/collections.hpp`'s `to_real_array1`/`to_real_array2`, not
  `to_array1`/`to_array2`, specifically because the *target* OCCT array's element
  type is `Standard_Real`, a distinct C++ type from `double` under
  `GRUNK_OCCT_WITH_ADOLC` (`Standard_Adouble`, not a typedef - see `../src/
  occt_ad_support.hpp`'s own comment) - `to_array1<double>` would build the wrong
  array type there. Needed once `GeomAPI_PointsToBSpline`/`Geom_BSplineCurve` (a
  real `TColStd_Array1OfReal`-taking constructor, not just a `TColgp_Array1OfPnt`
  one) actually had to compile under AD - an earlier version of this generator
  instead excluded any such overload from the AD build entirely, `#if
  !defined(GRUNK_OCCT_WITH_ADOLC)`-gated, deferring this exact fix.
  - **Parameter-only, deliberately**: `Param.kind()` only recognizes an array
    canonical type when `self.name` is non-empty, which is true for every genuine
    parameter but never for the synthetic `Param` `Callable.rejection_reason` builds
    to check a *return* type - so a method/constructor **returning** an array type
    (`Geom_BezierCurve::Poles() const -> const TColgp_Array1OfPnt&`, `Weights()`)
    stays rejected, same as a **mutable-reference** array out-param
    (`Poles(TColgp_Array1OfPnt& P)`) - unlike a registered object/`Handle`'s own
    non-const-reference out-param support above, `to_array1`/`from_array1` don't
    have an in-place-mutation story, and no concrete binding has forced a decision
    on either yet. See `../DESIGN.md`'s own note on this boundary.
- **Type registration spans the whole config, not just one module**: `main()`
  computes `registered_types`/`registered_enums` as the union of every module's
  discovered class/enum names *before* processing any of them (see "Lifting the
  one-class-per-header assumption" below for how they're discovered), so a later
  module's callables can reference an earlier module's types (e.g. `geom`'s
  methods take/return `gp_Pnt`/`gp_Vec`/`gp_Trsf`) - matches legacy grocc's
  cross-module type visibility. Every header across the whole config is parsed and
  walked exactly once (`parse_headers()`'s `ParsedHeader` cache, keyed by header
  filename) and reused for both this whole-config pass and each module's own
  `process_module()` call - re-parsing would roughly double an already
  non-trivial cost once `headers` can glob-expand to dozens of files (see
  "Wildcard header patterns" below).
- **Lifting the one-class-per-header assumption**: an earlier version of the
  generator assumed each header defined exactly one class, named for the header's
  own filename stem (`gp_Pnt.hxx` -> `gp_Pnt`) - true for hand-picked headers, but
  not a safe assumption once `headers` can be a glob (below): a matched header
  might define a
  typedef instead of a real class (`gp_Vec2f.hxx`, just
  `typedef NCollection_Vec2<Standard_ShortReal> gp_Vec2f;`), an enum, a namespace,
  or occasionally more than one class. `discover_entities()` replaces the
  filename-stem guess with an actual AST walk: every class/struct, enum, and
  namespace declared *directly* in a given header (not nested inside another
  class/namespace - `cursor.semantic_parent`/lexical position matter here, not
  just the file location, since a class/enum/namespace nested one level deeper
  would otherwise be picked up too) becomes its own `Entity`, independent of the
  header's filename. A header can now contribute zero, one, or several entities;
  `ModuleResult.entity_names` (discovery order) replaces the old
  `[Path(h).stem for h in headers]` computation everywhere it was used.
  - **A real correctness pitfall found doing this**: an explicit template
    specialization defined directly in a header (`gp_TrsfNLerp.hxx`'s
    `class NCollection_Lerp<gp_Trsf> { ... };`) has an ordinary `CLASS_DECL`
    cursor kind, and `cursor.spelling` is just the bare template name
    (`"NCollection_Lerp"`, no argument) - naively trusting it would register the
    specialization as if it were a plain, template-free class by that name, which
    doesn't compile (`register_type<NCollection_Lerp, ...>` needs a template
    argument). `cursor.type.spelling`, unlike `.spelling`, *does* include the
    argument (`"NCollection_Lerp<gp_Trsf>"`) - `discover_entities()` treats any
    mismatch between the two as "this is a specialization, not a plain class" and
    skips it, which also matches this project's existing "no `NCollection` type is
    ever exposed to Lua directly" policy (see `../DESIGN.md`'s own note on
    collections) rather than introducing an exception to it.
  - **Performance**: `discover_entities()` (and `find_class_definition()`, used by
    the base-chain walk) use `cursor.get_children()` - direct children of the
    translation unit only - rather than `cursor.walk_preorder()`, which recurses
    into *every* cursor's entire subtree (function bodies, nested expressions,
    template instantiation internals). Since every entity either function looks
    for is, by construction, a direct child of the translation unit already
    (`#include` textually inserts a header's top-level declarations at that point,
    it doesn't nest them under anything), the recursive walk's extra work bought
    nothing but cost - confirmed empirically on `TopoDS_Shape.hxx` (which
    transitively includes a fair amount of OCCT/STL): `walk_preorder()` visits
    ~212,000 cursors and takes ~0.6s; `get_children()` visits ~850 and takes
    ~0.01s. This is the difference between a full config regeneration finishing in
    seconds and not finishing inside a 10-minute timeout at all once `gp`'s
    headers list grew from 5 hand-picked files to a `gp_*.hxx` glob (~40 files,
    parsed/walked twice each in the naive two-pass design this was first
    implemented with, before the `ParsedHeader` cache above eliminated the second
    pass too).
- **Wildcard header patterns** (the same convention legacy grocc's own
  `config.yml.in` used - see its `gp_*.hxx`): a module's `headers` entries accept
  glob patterns (containing `*`, `?`, or `[`), expanded via `expand_headers()`
  against `--occt-include-dir` and sorted for a deterministic, reviewable diff -
  or a literal filename, kept as-is (and required to exist - a typo here is almost
  certainly a mistake, not something to silently ignore). `gp`'s own config
  changed from five hand-picked headers to `gp_*.hxx` wholesale as the concrete
  first case (see `../DESIGN.md`'s own write-up of what this retroactively
  unlocked in *other* modules, not just `gp` itself).
  - **A matched header that discovers nothing** (a typedef-only header like
    `gp_Vec2f.hxx`, above) is skipped quietly if it came from a glob
    (`expand_headers()` tracks which matched headers were glob-sourced
    specifically for this), but still a hard `RuntimeError` if it was named
    literally - preserves the original "a header with nothing
    registrable is probably a typo" safety net for the common (still
    hand-picked-header) case, while not making every wildcard module responsible
    for enumerating its own typedef-only headers as false-positive exceptions.
  - **Known hazard, not fully solved, documented rather than silently accepted**:
    a prefix-style glob can sweep in an unrelated header family sharing the same
    filename prefix - legacy grocc's own generated output shows this actually
    happening (`TopoDS*.hxx` also matching `TopoDSToStep_*.hxx`, a completely
    different OCCT package, per the investigation that informed this feature).
    The blacklist mechanism (a bare `ClassName`/`EnumName`) is the way to exclude
    something an over-eager glob picked up, same as excluding anything else this
    generator shouldn't bind - there's no automatic "this looks unrelated"
    detection.
- **Enum support**: a plain C-style enum (`enum GeomAbs_Shape { ... };`,
  OCCT's own convention throughout - never `enum class`) discovered by
  `discover_entities()` is registered via sol2's own `new_enum(...)` rather than
  `register_type<T, Flags>()` (enums aren't usertypes - no constructors, no
  methods, no `add_bases`). Each enumerator's own bare C++ name doubles as its
  Lua-side string key and its value (`occt.GeomAbs_Shape.GeomAbs_C0`), emitted
  as `sol::table(ns.table()).new_enum("GeomAbs_Shape", "GeomAbs_C0", GeomAbs_C0,
  ...)`. Two things worth knowing if this needs revisiting:
  - `grunk::plugin_namespace` has no dedicated enum-registration API - `.table()`
    returns the underlying `sol::table` (grunk's own raw namespace table), but as
    `const sol::table&`, and sol2's `new_enum(...)` is a non-`const` method (it
    mutates the table by inserting the new enum's own sub-table). Constructing a
    fresh `sol::table` value from the `const&` (`sol::table(ns.table())`) sidesteps
    the mismatch for free: `sol::table` has reference semantics (a lightweight
    handle into the Lua registry, not a deep copy of the table's contents), so the
    copy is cheap and mutating it still mutates the *same* underlying Lua table -
    no new grunk API needed.
  - This sol2 version's variadic `new_enum(name, "Key1", Value1, "Key2", Value2,
    ...)` overload needs **no explicit enum-type template argument** - it deduces
    the enum type from the value arguments themselves. (A separate, unrelated
    `new_enum<T, read_only>(name, initializer_list<pair<string_view, T>>)`
    overload does take one, but isn't used here.)
  - Once registered, an enum name behaves exactly like `Standard_Integer`/
    `Standard_Boolean` for parameter/return-type purposes (see the type list
    above) - passed through as a plain value, no conversion needed in the emitted
    lambda body.
- **Default-argument expansion** (the same trick legacy grocc's own generator
  used): a C++ declaration with N trailing default-valued parameters (e.g.
  `BRepBuilderAPI_Transform(shape, trsf, Copy = Standard_False)`, `BRepAlgoAPI_Cut
  (S1, S2, theRange = Message_ProgressRange())`) is emitted as multiple overloads,
  one per reachable arity, instead of the earlier all-or-nothing behavior (accept
  only if every parameter is individually supported, otherwise reject the whole
  callable). Crucially, the generator never needs to know a default's actual
  *value* - a shorter overload simply omits the trailing parameter(s) from the
  emitted call expression and lets C++ itself apply the real default at that call
  site, exactly as if Lua code had never provided anything for it.
  - `param_has_default()` detects a default via `PARM_DECL`'s own child cursors:
    every parameter has a `TYPE_REF`/`TEMPLATE_REF` child for its *type* (or none,
    for a bare fundamental type) - a *default* shows up as an additional, distinct
    child kind (`CXX_BOOL_LITERAL_EXPR`, `INTEGER_LITERAL`, `FLOATING_LITERAL`,
    `DECL_REF_EXPR` for an enum-constant default, `UNEXPOSED_EXPR` for a
    temporary-object default like `Message_ProgressRange()`) - confirmed
    empirically against real OCCT headers, not from documentation (libclang has no
    dedicated "has a default" query on a parameter cursor).
  - `Callable.accepted_arities()` computes the reachable range directly: `m` is the
    length of the longest prefix of individually-supported parameters (the same
    check that used to be all-or-nothing), `s` is the start of the longest
    trailing run of defaulted parameters. Because C++ requires every defaulted
    parameter to be trailing, the reachable arities always form one contiguous
    range `[s, m]` - `s > m` means the first unsupported parameter has no default
    to fall back on, i.e. today's plain rejection, unchanged.
  - An unsupported *and* defaulted trailing parameter doesn't need to be
    individually understood at all, since it's never touched: e.g.
    `GeomAPI_PointsToBSpline(Points, DegMin=3, DegMax=8, Continuity=GeomAbs_C2,
    Tol3D=1e-3)` unlocks 3 usable overloads (`Points`, `Points, DegMin`, `Points,
    DegMin, DegMax`) purely from this feature, even though `GeomAbs_Shape` isn't a
    registered enum anywhere yet - `Continuity`/`Tol3D` simply aren't reachable
    until it is, without blocking the three that already are.
  - Not deduplicated against other, independently-declared overloads of the same
    method that happen to erase to an identical parameter-kind sequence at some
    arity - not yet hit in practice (would show up as a real sol2/C++ compile
    error, not silently); re-derive a targeted fix (most likely a blacklist entry
    for whichever narrower overload should lose) if it ever is, per this project's
    own "confirmed by building, not assumed" policy.
- **`extra_includes`** (a module's config, optional, the same convention legacy
  grocc's own `config.yml.in` used): extra `#include <...>` lines added to the generated
  `.cpp` (never the `.hpp`, and never a source of new registrable classes - these
  headers are *not* walked for callables) for a type this module's callables use
  only by reference/pointer, where this module's *own* headers therefore only
  forward-declare it (e.g. `BRepBndLib.hxx`'s `class TopoDS_Shape;`, since a
  `const TopoDS_Shape&` parameter needs no more from `BRepBndLib.hxx`'s own point
  of view). sol2's usertype binding needs the type's *complete* definition in
  *this* translation unit regardless of it already being fully defined and
  registered by a different module's own `.cpp` (`generated/topods.cpp` for
  `TopoDS_Shape`) - omitting it fails with a real "invalid use of incomplete type"
  compile error (found on `bnd`, the first module needing it - see
  `codegen/config.yml`'s own comment there).
- **`translation_units`** (top-level config, optional - see `../DESIGN.md`'s own
  "Translation-unit grouping" section): controls how `CMakeLists.txt` groups `generated/*.cpp` files into
  compiler invocations, written by `generate.py` as `generated/translation_units.txt`
  (one line per translation unit, a space-separated list of module names, or a single
  line `*` for "everything in one"). `mode: separate` (the default) gives every
  module its own translation unit; `mode: unity` reproduces this project's original
  one-translation-unit-for-everything design. `groups` (a list of lists of module
  names, consulted under either mode) names sets of modules that must share a
  translation unit regardless - required whenever one module returns another
  module's `Handle(X)` by value (see `../DESIGN.md`'s "Handle(X) return type requires
  being in the same translation unit as X"); `codegen/config.yml`'s own comment
  tracks the confirmed cases. See `compute_translation_units()` in `generate.py` for
  the exact algorithm and `../DESIGN.md`'s "Translation-unit grouping" for why this
  exists at all (a single all-modules TU measured at ~12-13GB RAM, reliably
  OOM-killing CI).
- **`automagic_flags::none`** is applied to every registered type unconditionally (see
  `../DESIGN.md`'s policy) - the generator does not (yet) have a mechanism for opting
  a specific type into fuller automagic.
- **Blacklist**: `ClassName::MethodName` skips one method (all its overloads); a bare
  `ClassName` (or `EnumName`) skips the whole entity entirely - no
  `register_type`/`new_enum` call at all, checked *before* any of its own
  callables/enumerators are even looked at (an earlier implementation bug
  meant a bare-blacklisted class still ended up with an empty, useless
  `register_type<...>()` call with zero constructors/methods - fixed as part of
  the discovery rewrite above, though never actually exercised by a real config
  entry before then). Each entry in `config.yml` should carry an inline comment
  explaining why - re-derive the reason fresh, don't copy a legacy grocc
  blacklist reason without checking it still applies.
- **Operator overloads and friend free functions** (added for grunk-adolc's own
  native ADOL-C binding - see its own `DESIGN.md` - generic, not adouble-specific;
  no OCCT module needed this before, but several turned out to benefit once it
  existed - see below): a *member* operator is accepted only if its C++ token has
  a real Lua metamethod slot to occupy - `BINARY_OPERATOR_META`/
  `UNARY_OPERATOR_META` in `generate.py` list exactly which (`+ - * / == < <=` and
  unary `-`). Several common C++ operators are deliberately *never* emitted, not a
  gap to eventually close: `!= > >=` (Lua derives these from `__eq`/`__lt`/`__le`
  itself, registering them natively would be redundant), compound assignment and
  increment/decrement (Lua has no metamethod for either - `x += y` isn't valid Lua
  syntax at all), and conversion operators (`operator double()` etc. - a different
  mechanism entirely, not attempted).

  A **friend** function declared inside a class body (a real C++ idiom, not
  something contrived for this - ADOL-C's own `adtl::adouble` uses it throughout,
  for `sin`/`cos`/... and for the *reversed*-operand form of its arithmetic/
  comparison operators, e.g. `friend adouble operator+(double, const adouble&)`
  alongside the member `adouble::operator+(double) const`) has namespace scope for
  the function itself despite its lexical nesting - `collect_friend_functions()`
  finds non-operator friends this way (attributed to the class's *enclosing
  namespace*, e.g. `adtl::sin`, not the class), while a **reversed-operand
  operator sibling** (`5 + x` as well as `x + 5`) is synthesized separately in
  `process_module()`, and only after **confirming** (via `find_friend_operator_
  reversed()`, a real AST check) that the matching friend operator actually
  exists - never assumed just because the member form does. Found the hard way,
  regenerating this project's own existing OCCT config while building this: ADOL-C's
  `adouble` defines all four reversed arithmetic forms uniformly, but OCCT's own
  geometric types don't - `gp_XYZ::operator/(Standard_Real) const` (scale down) has
  no reversed friend at all (dividing a scalar *by* a vector isn't a defined
  operation), while `gp_Mat::operator*` does (scalar * matrix is standard linear
  algebra). Assuming reversal always exists compiled cleanly for `adtl::adouble`
  but broke on `gp_XYZ`'s own division - confirming it via the real AST instead of
  guessing was the fix.

  Concrete effect on existing OCCT modules, entirely as a side effect of adding
  this for grunk-adolc's sake, not sought out deliberately: `gp_Vec`/`gp_Vec2d`/
  `gp_XY`/`gp_XYZ`/`gp_Mat`/`gp_Mat2d`/`gp_Quaternion`/`gp_GTrsf`/`gp_GTrsf2d`/
  `gp_Trsf`/`gp_Trsf2d`/`gp_Dir`/`gp_Dir2d` all gained real Lua operator overloading
  (`+`/`-`/`*`/unary `-`, several with a working reversed-scalar form too) that was
  previously silently skipped entirely; `TopLoc_Location` gained `*`/`/`/`==`;
  `TopoDS_Shape` gained `==`. Verified: full regeneration, clean rebuild, all 37
  tests plus both example recipes pass with zero regressions.
- **Static-only "namespace classes"** (see `../DESIGN.md`): OCCT
  expresses this concept two different ways, both handled: a genuine C++ `namespace`
  with free functions (`TopoDS.hxx`'s `namespace TopoDS { const TopoDS_Face&
  Face(const TopoDS_Shape&); ... }`), and an ordinary `class` whose methods happen to
  all be `static` (`BRepTools`). Both are detected automatically per header (try
  `CLASS_DECL`/`STRUCT_DECL` first, fall back to a `NAMESPACE` cursor - no config
  schema change needed) and rendered identically: no "self", called fully-qualified
  (`ClassOrNamespace::Method(args)`). **Chosen naming convention: flattened**
  (`occt.ClassOrNamespace_Method(args)`, e.g. `occt.BRepTools_Write(shape, file)`) -
  *not* a nested Lua table (`occt.BRepTools.Write(...)`). The nested-table option was
  seriously considered (and works fine for a genuine class's statics, via
  `register_type<T>` with zero constructors and `add_member_function` for each
  static), but **cannot work for a real C++ namespace at all** - `TopoDS` has no
  type to `register_type<>()` on - so the flattened convention is the only one that
  generalizes to both cases uniformly, which settled the spike.
  - **Naming collision, handled specially**: `TopoDS`'s own downcast helpers
    (`TopoDS::Face`/`Edge`/`Vertex`/...) each compose to the exact same name as the
    leaf type they cast *to* (`TopoDS_Face`, already a registered class) - not a
    hypothetical, every single one of `TopoDS.hxx`'s functions collides this way.
    Registering `occt.TopoDS_Face` as a plain free function would clobber the
    `TopoDS_Face` usertype's own table. Detected (composed name already in this
    module's `registered_types`) and redirected: attached as that type's own
    `.DownCast(shape)` member instead (`occt.TopoDS_Face.DownCast(shape)`) - mirrors
    the `Handle(T)::DownCast` naming used elsewhere (`src/geom/geom_extras.hpp`)
    for one consistent Lua-side idiom regardless of which OCCT downcast mechanism
    is underneath. **Only detected within a single module's own class set** - a
    collision between a static/namespace function in one module and a class
    registered in a *different* module wouldn't be caught (not yet hit in practice).
- **Toolchain include-path discovery is GCC/Clang-only** (`system_cxx_include_dirs()`
  probes `$CXX`/`c++` with `-E -x c++ -v -`, parsing the "#include <...> search starts
  here" block - this doesn't work for MSVC). Not a blocker today since codegen is
  opt-in and its output is checked into git (regeneration only needs to happen on
  whatever platform a maintainer actually runs it from), but worth fixing before
  relying on Windows-run regeneration.
- **One clang parse per header** (no shared "compilation database"/precompiled
  headers) - the `ParsedHeader` cache (see "Type registration spans the whole
  config" above) already avoids re-parsing the same header twice across a whole
  run; sharing a compilation database/precompiled headers across *different*
  headers remains a possible further optimization if generation time ever
  becomes a real bottleneck.
