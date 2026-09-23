#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

"""grunk's shared code generator: parses a set of C++ headers (via libclang) driven
by a declarative per-plugin config (codegen/config.yml) and emits grunk plugin
registration code (generated/<module>.cpp) targeting grunk's register_type/
add_constructors/add_member_function API. Shared by every native grunk plugin that
wraps a third-party C++ library this way (grunk-occt for OpenCASCADE, grunk-adolc
for ADOL-C's adtl.h, ...) - each plugin brings its own config.yml (headers, module
list, blacklist, and a `helper_namespace` naming its own conversion-helper namespace
- see emit_lambda), not a shared one.

See grunk-occt's own codegen/README.md for the full design (what's accepted, what's
rejected and why, known gaps) - deliberately narrow in scope: constructors/instance
methods only, and only a small allowlist of "simple" parameter/return types can be
bound generically. Anything else is silently skipped and reported in the summary
printed at the end of a run - review that output when adding a new module, since
it's the primary way to discover what needs a hand-written escape hatch.

A module's `headers` list accepts glob patterns (e.g. "gp_*.hxx") as well as literal
filenames, expanded against --include-dir - see expand_headers(). Each matched
header is scanned for every class/struct/enum/namespace *directly* declared in it (not
just one matching the filename stem, and not anything nested or only transitively
included) - see discover_entities().
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

import clang.cindex as cindex
import yaml


def system_cxx_include_dirs() -> list[str]:
    """libclang (unlike invoking `clang++`/`g++` directly) doesn't auto-discover the
    system C++ toolchain's own header search path (type_traits, utility, ...) - probe
    the actual compiler (see CMAKE_CXX_COMPILER/$CXX, falling back to "c++") for it,
    the same way `c++ -E -x c++ -v -` reports it. Linux/macOS only for now (MSVC
    doesn't support this invocation) - see codegen/README.md's "Known gaps"; not a
    blocker since codegen is opt-in and generated/ output is checked into git."""
    compiler = os.environ.get("CXX", "c++")
    try:
        proc = subprocess.run(
            [compiler, "-E", "-x", "c++", "-v", "-"],
            input="", capture_output=True, text=True, check=False,
        )
    except FileNotFoundError:
        return []
    output = proc.stderr
    match = re.search(r"#include <\.\.\.> search starts here:\n(.*?)\nEnd of search list\.", output, re.S)
    if not match:
        return []
    return [line.strip() for line in match.group(1).splitlines() if line.strip()]

def expand_headers(patterns: list[str], include_dir: Path, exclude_headers: list[str] = ()) -> tuple[list[str], set[str]]:
    """Expands a module's `headers` list: a literal filename is kept as-is (and must
    exist), a glob pattern (contains any of *?[) is matched against
    include_dir and expanded to every matching filename, sorted for a
    deterministic, reviewable diff. Deduplicates across all patterns in the list
    (in case two overlap) while preserving first-seen order.

    exclude_headers drops specific glob-matched filenames before they're ever
    parsed - needed for a header that doesn't even *parse* standalone (a hard clang
    error, not a rejected/blacklisted symbol - blacklist only excludes a symbol
    *after* its header has already parsed successfully, so it can't help here). Found
    with BRepBuilderAPI_CellFilter.hxx: it instantiates
    NCollection_CellFilter<gp_XY> without gp_XY's own header, incomplete-type errors
    result - a real, narrow header-level parse gap, not a symbol this generator
    should even attempt to discover. Only ever applies to headers a glob actually
    matched (a literal, hand-picked header listed in exclude_headers as well as
    headers would be a config contradiction, not a real use case, so this isn't
    guarded against explicitly).

    Returns (headers, glob_matched) - glob_matched is the subset that came from a
    wildcard rather than being named explicitly, which process_module uses to
    decide how strict to be about a header defining nothing this generator cares
    about (see its own docstring): a hand-picked literal header with zero
    class/struct/enum/namespace definitions is almost certainly a typo/mistake and
    stays a hard error, but a glob is *expected* to occasionally sweep in a header
    that doesn't declare a real type at all - e.g. gp_*.hxx matching gp_Vec2f.hxx,
    which is only `typedef NCollection_Vec2<Standard_ShortReal> gp_Vec2f;` - which
    should be skipped quietly rather than failing the whole run.

    Known hazard (see DESIGN.md): a prefix-style glob can accidentally sweep in an
    unrelated header family with the same filename prefix (e.g. `TopoDS*.hxx` would
    also match `TopoDSToStep_*.hxx`, a completely different OCCT package) - the
    blacklist mechanism (a bare ClassName) is the way to exclude something an
    over-eager glob picked up, same as excluding anything else this generator
    shouldn't bind."""
    excluded = set(exclude_headers)
    seen: set[str] = set()
    result: list[str] = []
    glob_matched: set[str] = set()
    for pattern in patterns:
        is_glob = any(ch in pattern for ch in "*?[")
        if is_glob:
            matches = sorted(p.name for p in include_dir.glob(pattern) if p.name not in excluded)
            if not matches:
                raise RuntimeError(f"header glob {pattern!r} matched no files in {include_dir}")
        else:
            if not (include_dir / pattern).exists():
                raise RuntimeError(f"header {pattern!r} not found in {include_dir}")
            matches = [pattern]
        for m in matches:
            if is_glob:
                glob_matched.add(m)
            if m not in seen:
                seen.add(m)
                result.append(m)
    return result, glob_matched


STANDARD_REAL = "Standard_Real"
# "double"/"unsigned int"/"bool" (bare fundamental spellings, as opposed to OCCT's
# own Standard_Real/Standard_Integer/Standard_Boolean typedefs above) are only ever
# actually written that way in a header this generator targets *outside* OCCT
# itself (e.g. ADOL-C's own adtl.h, for grunk-adolc's own native binding - see
# codegen/README.md's "Operator overloads and friend free functions") - OCCT's own
# headers consistently spell these through their own typedefs, never bare, so
# adding them here doesn't change any existing OCCT-targeting config's behavior.
PASSTHROUGH_TYPES = {"Standard_Integer", "Standard_Boolean", "double", "unsigned int", "bool", "size_t"}
TRANSIENT_ROOT = "Standard_Transient"
HANDLE_PATTERN = re.compile(r"^opencascade::handle<\s*(\w+)\s*>$")

# Operator overloads (see codegen/README.md's "Operator overloads and friend free
# functions"): C++ operator token -> sol2 meta_function name. Deliberately a small
# subset of what C++ allows overloading, not "every operator token" - each entry
# here corresponds to a real Lua metamethod slot; several common C++ operators have
# *no* Lua equivalent at all and are silently skipped (never added to either map
# below, falling through collect_class_callables' own "not a recognized operator"
# branch exactly like an unsupported one): operator!=/>/>= (Lua derives these from
# __eq/__lt/__le itself - see lua.org's manual on operator inheritance for
# comparisons - registering them natively would be redundant, not wrong, but
# pointless surface area), compound assignment (+=, -=, ...- Lua has no
# assignment-operator metamethod, `x += y` isn't valid Lua syntax at all),
# increment/decrement (same reason), plain operator= (assignment has its own
# grunk-level semantics via sol2's usertype machinery, not a metamethod), and
# conversion operators (operator double() etc. - would need a different mechanism
# entirely, not attempted here).
BINARY_OPERATOR_META = {
    "+": "addition",
    "-": "subtraction",
    "*": "multiplication",
    "/": "division",
    "==": "equal_to",
    "<": "less_than",
    "<=": "less_than_or_equal_to",
}
# Unary operators - a *different* map since the same token ("-") means something
# else with zero arguments than with one (subtraction is binary, unary_minus takes
# no other operand) - collect_class_callables tells them apart by each candidate
# operator method's own argument count, not by token alone.
UNARY_OPERATOR_META = {
    "-": "unary_minus",
}

# NCollection_Array1<T>/NCollection_Array2<T> (e.g. TColgp_Array1OfPnt, TColStd_Array1OfReal)
# are typedefs, invisible as such to a parameter's own (non-canonical) spelling - only
# libclang's *canonical* type resolution reveals the actual template ("const
# NCollection_Array1<gp_Pnt> &"), which is what Param.canonical carries and these
# patterns match against (see Param.kind's "Array1OfX/Array2OfX" section below and
# codegen/README.md's "NCollection_Array1/2 support").
ARRAY1_PATTERN = re.compile(r"^NCollection_Array1<\s*(.+?)\s*>$")
ARRAY2_PATTERN = re.compile(r"^NCollection_Array2<\s*(.+?)\s*>$")
# Canonical resolution also resolves Standard_Real/Standard_Integer/Standard_Boolean
# themselves away to their own underlying fundamental type (double/int/bool) - an
# array element's spelling never comes back as "Standard_Real", only "double".
REAL_ELEMENT_SPELLINGS = {"double", "float"}
PASSTHROUGH_ELEMENT_SPELLINGS = {"int", "bool"}


def array_element_supported(elem: str, registered_types: set[str]) -> bool:
    """Whether elem - an NCollection_Array1/2's own already-canonical template
    argument spelling, e.g. "gp_Pnt" or "double" - is itself a collection element
    type this generator can convert via src/collections.hpp's to_array1/to_array2.
    Deliberately does NOT extend AD-portability's #ifdef GRUNK_OCCT_WITH_ADOLC
    dual-emission to array elements (see codegen/README.md's "Known gaps") - a
    "real" array element is always plain double, in both AD and non-AD builds."""
    return (
        elem in REAL_ELEMENT_SPELLINGS
        or elem in PASSTHROUGH_ELEMENT_SPELLINGS
        or elem in registered_types
    )


def array_element_type(canonical: str, pattern: re.Pattern) -> str:
    """Re-extracts the same element type array_element_supported already found
    supported, for emission (see emit_lambda) - called only after kind() has
    already matched, so the match here is guaranteed to succeed."""
    m = pattern.match(strip_type(canonical))
    assert m is not None, f"expected {canonical!r} to match {pattern.pattern!r}"
    return m.group(1).strip()

AD_MACRO = "GRUNK_OCCT_WITH_ADOLC"

GENERATED_BANNER = """\
// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

// AUTO-GENERATED by codegen/generate.py from codegen/config.yml - do not edit by
// hand, your changes will be overwritten the next time this module is regenerated.
// See DESIGN.md's "Generated-code policy" and codegen/README.md.
"""


def strip_type(spelling: str) -> str:
    """Base type name with const/reference/whitespace stripped, e.g.
    "const gp_Vec &" -> "gp_Vec"."""
    s = re.sub(r"\bconst\b", "", spelling)
    s = s.replace("&", "")
    return re.sub(r"\s+", " ", s).strip()


@dataclass
class Param:
    spelling: str
    name: str
    # Canonical (typedef-resolved) spelling, e.g. "const TColgp_Array1OfPnt &" ->
    # "const NCollection_Array1<gp_Pnt> &" - only populated for genuine parameters
    # (see collect_class_callables/collect_namespace_callables), never for the
    # synthetic Param Callable.rejection_reason constructs to check a return type;
    # kind() uses that (name == "") to deliberately keep NCollection_Array1/2
    # support parameter-only for now - see kind()'s own comment.
    canonical: str = ""
    # Whether the real C++ declaration gives this parameter a default value (e.g.
    # `Standard_Boolean Copy = Standard_False`) - see param_has_default() for how
    # this is actually detected via libclang, and Callable.accepted_arities() for
    # what it unlocks (default-argument expansion). Only ever True for a
    # genuine parameter (populated by collect_class_callables/
    # collect_namespace_callables), never for the synthetic Param
    # Callable.rejection_reason/accepted_arities construct to check a return type.
    has_default: bool = False

    @property
    def base(self) -> str:
        return strip_type(self.spelling)

    @property
    def is_mutable_ref(self) -> bool:
        return "&" in self.spelling and "const" not in self.spelling

    def kind(self, registered_types: set[str], registered_enums: set[str]) -> str | None:
        # Non-const-reference ("out") parameters ARE supported here, but only when
        # the base type is itself an OCCT object/Handle usertype - Lua/sol2 userdata
        # for a registered class already has stable, mutable identity (the same
        # userdata the caller passed in gets mutated in place, exactly like OOP
        # mutation works in Lua natively), so `void D0(Standard_Real U, gp_Pnt& P)
        # const` binds and behaves correctly with zero special-casing. What's NOT
        # supported is a non-const reference to a *primitive* (Standard_Real/
        # Standard_Integer/Standard_Boolean) - Lua has no mutable-number-by-reference
        # concept, there is no userdata to mutate in place for a bare number. See
        # codegen/README.md's "Non-const-reference ('out') parameters" section - this
        # is a real, common OCCT idiom (D0/D1/D2/D3, Coord's 3-out-param overload,
        # ...), not a corner case, and the *recommended* way to consume it from a
        # recipe is grunk's own "module" mechanism (raw/undecorated Lua calls inside
        # a module function, whose own return value is what gets tracked
        # parametrically) - not something this generator needs to encode itself.
        if self.base == STANDARD_REAL:
            return None if self.is_mutable_ref else "real"
        if self.base in PASSTHROUGH_TYPES:
            return None if self.is_mutable_ref else "passthrough"
        # A plain C-style enum (e.g. GeomAbs_Shape) - sol2 marshals it as a plain
        # value with zero special-casing needed in the emitted lambda body (see
        # codegen/README.md's "Enum support"), exactly like Standard_Integer/
        # Boolean above - reuses the "passthrough" kind rather than a new one.
        if self.base in registered_enums:
            return None if self.is_mutable_ref else "passthrough"
        if self.base in registered_types:
            return "object"
        # Handle(X) (i.e. opencascade::handle<X>) - accepted only if X is itself a
        # type being registered in this run (see Param docs / codegen/README.md's
        # section on Standard_Transient/Handle(T) support). Bridged to Lua via
        # occt_sol_traits.hpp's sol::unique_usertype_traits<opencascade::handle<T>>
        # specialization - no special conversion needed in the emitted lambda body,
        # the type is simply passed/returned as-is (mutable reference included).
        m = HANDLE_PATTERN.match(self.base)
        if m and m.group(1) in registered_types:
            return "handle"
        # NCollection_Array1<X>/NCollection_Array2<X> (see codegen/README.md's
        # "NCollection_Array1/2 support") - by value or const-reference only
        # (is_mutable_ref check mirrors Standard_Real's own out-param rule above: no
        # Lua-side identity to mutate a bare std::vector<X> in place through, unlike
        # a registered object/Handle). Parameter-only: self.name is empty only for
        # the synthetic Param Callable.rejection_reason builds to check a *return*
        # type, so an array-typed return stays rejected exactly as before - see
        # codegen/README.md's "Known gaps" for why (no decided Lua-facing shape yet
        # for a from_array1/2-converted return value).
        if self.name and self.canonical:
            canon_base = strip_type(self.canonical)
            m1 = ARRAY1_PATTERN.match(canon_base)
            if m1 and array_element_supported(m1.group(1), registered_types):
                return None if self.is_mutable_ref else "array1"
            m2 = ARRAY2_PATTERN.match(canon_base)
            if m2 and array_element_supported(m2.group(1), registered_types):
                return None if self.is_mutable_ref else "array2"
        return None


@dataclass
class Callable:
    class_name: str
    kind: str  # "constructor" | "method" | "operator"
    cpp_name: str
    params: list[Param]
    return_spelling: str | None  # None for constructors
    is_const: bool
    is_static: bool = False  # static class method, or a namespace free function - no "self"
    # Only set for kind == "operator" (see collect_class_callables' own operator
    # handling and codegen/README.md's "Operator overloads and friend free
    # functions"): meta_function is the sol2 sol::meta_function member name
    # (e.g. "addition"); operator_token is the real C++ operator token (e.g. "+"),
    # used by emit_operator_lambda to build the actual C++ expression - self OP
    # other for a normal member operator, other OP self for the synthesized
    # reversed-operand sibling process_module() adds for a binary operator whose
    # only parameter is a plain literal (double) rather than the class's own type.
    meta_function: str | None = None
    operator_token: str | None = None

    def rejection_reason(self, registered_types: set[str], registered_enums: set[str]) -> str | None:
        for p in self.params:
            if p.kind(registered_types, registered_enums) is None:
                return f"unsupported parameter type '{p.spelling.strip()}'"
        if self.return_spelling is not None and self.return_spelling.strip() != "void":
            if Param(self.return_spelling, "").kind(registered_types, registered_enums) is None:
                return f"unsupported return type '{self.return_spelling.strip()}'"
        return None

    def accepted_arities(self, registered_types: set[str], registered_enums: set[str]) -> list[int]:
        """Every number of leading parameters this callable can be called with from
        Lua, in ascending order - normally just [len(self.params)] (today's
        all-or-nothing behavior, when nothing is defaulted), but more than one value
        when trailing parameters have real C++ default values: this generator
        doesn't need to know the *value* of a defaulted trailing parameter, only
        that it has one - a shorter overload can simply omit it from the emitted
        call expression and let C++ itself apply the real default at the actual
        call site (see param_has_default(); DESIGN.md's "Default-argument
        expansion"). Returns [] if nothing is
        reachable at all - either the return type itself is unsupported (no arity
        helps that), or even the shortest reachable prefix still needs an
        unsupported parameter that has no default to fall back on.

        Because C++ requires every defaulted parameter to be trailing (you can
        never have a non-default parameter after a default one), the reachable
        arities always form one contiguous range [s, m]: m is the length of the
        longest prefix of individually-supported parameters (kind() is not None -
        the same check rejection_reason() already made all-or-nothing), and s is
        the start of the longest trailing run of parameters that all have a
        default (so anything from there on can be dropped). s > m means the first
        unsupported parameter has no default to fall back on - nothing is
        reachable, same as today's plain rejection."""
        if self.return_spelling is not None and self.return_spelling.strip() != "void":
            if Param(self.return_spelling, "").kind(registered_types, registered_enums) is None:
                return []
        n = len(self.params)
        kinds = [p.kind(registered_types, registered_enums) for p in self.params]
        m = 0
        while m < n and kinds[m] is not None:
            m += 1
        s = n
        while s > 0 and self.params[s - 1].has_default:
            s -= 1
        if s > m:
            return []
        return list(range(s, m + 1))


@dataclass
class Entity:
    name: str
    cursor: cindex.Cursor
    kind: str  # "class" | "enum" | "namespace"


def discover_entities(tu: cindex.TranslationUnit, header_path: Path) -> list[Entity]:
    """Finds every class/struct, enum, and namespace declared *directly* in
    header_path (not nested inside another class, and not merely transitively
    #included from it) - see codegen/README.md's "Lifting the one-class-per-header
    assumption". Each becomes its own Entity, independent of the header's own
    filename: a header may define zero, one, or several (a real case once
    `headers` accepts glob patterns - see expand_headers() - rather than each
    header being hand-picked to match exactly one wanted class).

    Only *direct* children of the translation unit, or of a namespace that is
    itself one, are considered - one level of namespace nesting is followed (see
    below), but never a second (a namespace inside a namespace) or into a class
    body at all - this is what excludes a class's own protected/private nested
    helper classes (see codegen/README.md's "Known gaps") and a class-scoped
    member enum, without needing a separate filter for each. A class/struct must
    be a real definition (`is_definition()`, excludes forward declarations); an
    enum must additionally have a name (skips anonymous enums, which have no
    symbolic type to register). "std" is excluded by name even though it
    technically qualifies (headers using OCCT's hashing support commonly reopen
    `namespace std { template<> struct hash<...> {...}; }` right there, at true
    TU-child scope) - harmless either way (render_module skips every "namespace"
    entity's own registration regardless, and a plain namespace reopen has no free
    FUNCTION_DECLs for collect_namespace_callables to find), just noise in the
    discovered-entity list.

    One level of namespace nesting is followed - found needing this the hard way,
    not anticipated in advance: no OCCT header this generator has ever targeted
    wraps its classes in a namespace (OCCT uses prefix-based naming instead, e.g.
    `TopoDS_Shape`, never `namespace TopoDS { class Shape; }`), but ADOL-C's own
    `adtl::adouble` does - without this, `adouble` itself (a child of the `adtl`
    namespace, not of the translation unit) was never discovered as a "class"
    entity at all, silently registering none of its constructors/methods/
    operators and leaving every friend function nested inside it (see
    collect_friend_functions) permanently "unsupported parameter type 'const
    adouble &'" - *itself* never being a registered type. A second level of
    nesting (a namespace inside a namespace) is deliberately not followed - not
    needed by anything targeted so far, and each additional level compounds the
    performance cost the note below is about.

    Performance note: uses `cursor.get_children()` (direct children only, of the
    translation unit and - one level down - of a namespace within it), *not*
    `tu.cursor.walk_preorder()` (which recurses into every cursor's entire subtree
    - function bodies, nested expressions, template instantiation internals, the
    works). Since every entity this function looks for is by definition at most
    one level down already, walk_preorder()'s extra recursion buys nothing here
    but cost: for a single moderately-included header, it visits on the order of
    10^5 cursors where get_children() visits closer to 10^3 - confirmed
    empirically (0.6s vs 0.01s on TopoDS_Shape.hxx) - the difference between this
    finishing in well under a second and a full regen run not finishing inside a
    10-minute timeout at all. Found the hard way."""
    interesting_kinds = (
        cindex.CursorKind.CLASS_DECL,
        cindex.CursorKind.STRUCT_DECL,
        cindex.CursorKind.ENUM_DECL,
        cindex.CursorKind.NAMESPACE,
    )
    header_path_resolved = header_path.resolve()
    entities: list[Entity] = []

    def visit(cursors) -> None:
        for cursor in cursors:
            if cursor.kind not in interesting_kinds:
                continue
            loc_file = cursor.location.file
            if loc_file is None or Path(loc_file.name).resolve() != header_path_resolved:
                continue
            if cursor.kind in (cindex.CursorKind.CLASS_DECL, cindex.CursorKind.STRUCT_DECL) and cursor.is_definition():
                # Excludes an explicit template specialization defined right in a
                # header (a real case, not hypothetical: gp_TrsfNLerp.hxx defines
                # `class NCollection_Lerp<gp_Trsf> { ... };` directly) - its cursor.kind
                # is an ordinary CLASS_DECL and cursor.spelling is just the bare
                # template name ("NCollection_Lerp", no argument), which would
                # otherwise get registered as if it were a plain, template-free class
                # named that - cursor.type.spelling, unlike .spelling, does include the
                # template argument ("NCollection_Lerp<gp_Trsf>"), so a mismatch
                # between the two is exactly the signal a specialization is present.
                # Matches this project's existing "no NCollection type is ever exposed
                # to Lua directly" policy (see DESIGN.md's collections note) -
                # this isn't a new exclusion so much as that policy applying here too.
                #
                # .rsplit("::", 1)[-1] first: cursor.type.spelling is fully qualified
                # (e.g. "adtl::adouble" for a class one level inside a namespace -
                # see this function's own note on following namespace nesting),
                # cursor.spelling never is - comparing the two directly, unqualified,
                # would flag *every* namespaced class as a false-positive
                # "specialization" and silently drop it. No OCCT class this
                # generator has targeted is ever namespaced, so this never mattered
                # until ADOL-C's own adtl::adouble did - found the hard way
                # (registered_types came back empty, silently rejecting every
                # friend function that takes an adouble parameter, and no amount of
                # staring at the operator/friend-function code itself explained why -
                # adouble was never being discovered as a class at all).
                if strip_type(cursor.type.spelling).rsplit("::", 1)[-1] != cursor.spelling:
                    continue
                entities.append(Entity(cursor.spelling, cursor, "class"))
            elif cursor.kind == cindex.CursorKind.ENUM_DECL and cursor.is_definition() and cursor.spelling:
                entities.append(Entity(cursor.spelling, cursor, "enum"))
            elif cursor.kind == cindex.CursorKind.NAMESPACE and cursor.spelling != "std":
                entities.append(Entity(cursor.spelling, cursor, "namespace"))

    visit(tu.cursor.get_children())
    for entity in list(entities):
        if entity.kind == "namespace":
            visit(entity.cursor.get_children())
    return entities


def enum_constant_names(enum_cursor: cindex.Cursor) -> list[str]:
    """The enumerator names of an ENUM_DECL cursor, in declaration order (e.g.
    ["GeomAbs_C0", "GeomAbs_C1", ...] for GeomAbs_Shape) - see codegen/README.md's
    "Enum support" for how these become both the Lua-side key and the bare C++
    value in the emitted new_enum(...) call."""
    return [c.spelling for c in enum_cursor.get_children() if c.kind == cindex.CursorKind.ENUM_CONSTANT_DECL]


@dataclass
class ParsedHeader:
    tu: cindex.TranslationUnit
    entities: list[Entity]


def parse_headers(headers: list[str], include_dir: Path, clang_args: list[str],
                   cache: dict[str, ParsedHeader]) -> None:
    """Parses each header not already in cache and discovers its entities, storing
    both under cache[header] - shared by main()'s whole-config discovery pass and
    process_module()'s own per-module pass so each header is only ever parsed
    once, regardless of how many modules reference it. Parsing (and, for a
    heavily-included header, walking its AST) is the dominant cost here - Standard
    C++ header parsing runs in the ~0.1-0.5s range per header even for a single
    pass, so avoiding a second full pass over the same header roughly halves total
    runtime once a config has any overlap between the whole-config discovery scan
    and each module's own processing (which a wildcard-expanded config, with
    modules like "gc"/"geom" split apart, will have far more of than a
    one-header-per-module design would)."""
    index = cindex.Index.create()
    for header in headers:
        if header in cache:
            continue
        header_path = include_dir / header
        tu = index.parse(str(header_path), args=clang_args)
        diags = [d for d in tu.diagnostics if d.severity >= cindex.Diagnostic.Error]
        if diags:
            raise RuntimeError(f"clang errors parsing {header}:\n" + "\n".join(str(d) for d in diags))
        cache[header] = ParsedHeader(tu, discover_entities(tu, header_path))


# A PARM_DECL cursor's own children always include a REF cursor for its *type*
# (TYPE_REF for a named class/enum/typedef, TEMPLATE_REF for a template, none at
# all for a bare fundamental type like Standard_Real/size_t) - none of those three
# indicate a default value. Any *other* child kind appearing on a parameter is the
# default-value expression itself (confirmed empirically against real OCCT headers
# - see DESIGN.md's "Default-argument expansion": CXX_BOOL_LITERAL_EXPR for
# `Standard_Boolean Copy = Standard_False`, INTEGER_LITERAL/FLOATING_LITERAL for a
# numeric default, DECL_REF_EXPR for an enum-constant default like
# `ChFi3d_FilletShape FShape = ChFi3d_Rational`, UNEXPOSED_EXPR for a
# temporary-object default like `Message_ProgressRange theRange =
# Message_ProgressRange()`). libclang has no dedicated "has a default value" query
# on a parameter cursor, so this is the standard heuristic other libclang-based
# tools use for the same question.
NON_DEFAULT_CHILD_KINDS = {
    cindex.CursorKind.TYPE_REF,
    cindex.CursorKind.TEMPLATE_REF,
    cindex.CursorKind.NAMESPACE_REF,
}


def param_has_default(arg_cursor: cindex.Cursor) -> bool:
    return any(child.kind not in NON_DEFAULT_CHILD_KINDS for child in arg_cursor.get_children())


def collect_namespace_callables(namespace_cursors: list[cindex.Cursor], ns_name: str) -> list[Callable]:
    """Returns every free FUNCTION_DECL directly inside the given namespace
    fragment(s) as unfiltered, is_static=True Callables (see codegen/README.md's
    "Static-only 'namespace classes'" - a namespace free function and a class's
    static method are emitted identically: no self, ns_name::cpp_name(args)).

    Deduplicates by (spelling, parameter-type tuple) across every fragment - not a
    hypothetical: a header can reopen the same namespace more than once with a
    forward *declaration* in one place and the real *definition* in another (ADOL-C's
    own adtl.h does exactly this for makeNaN/makeInf/setNumDir/getNumDir - a bare
    prototype up top, the real `inline` bodies further down, in a second `namespace
    adtl { ... }` block) - both are independent FUNCTION_DECL cursors for the
    logically same function, and without this, both would be collected as separate
    overloads, generating the same lambda body twice. Never triggered by any OCCT
    header targeted so far (none of them reopen a namespace this way), so this is
    strictly additive - a single-fragment, single-declaration case (the only kind
    seen until now) has nothing to deduplicate against."""
    callables: list[Callable] = []
    seen: set[tuple[str, tuple[str, ...]]] = set()
    for target in namespace_cursors:
        for child in target.get_children():
            if child.kind != cindex.CursorKind.FUNCTION_DECL:
                continue
            if child.spelling.startswith("operator"):
                continue  # operators not handled yet - see codegen/README.md's "Known gaps"
            args = list(child.get_arguments())
            key = (child.spelling, tuple(arg.type.spelling for arg in args))
            if key in seen:
                continue
            seen.add(key)
            params = [Param(arg.type.spelling, arg.spelling or f"arg{i}", arg.type.get_canonical().spelling,
                             has_default=param_has_default(arg))
                      for i, arg in enumerate(args)]
            callables.append(Callable(ns_name, "method", child.spelling, params,
                                       child.result_type.spelling, is_const=False, is_static=True))
    return callables


def find_class_definition(tu: cindex.TranslationUnit, class_name: str) -> cindex.Cursor | None:
    """Finds the CLASS_DECL/STRUCT_DECL definition of class_name anywhere in the
    translation unit - used to walk base classes from other, transitively-included
    headers (e.g. Geom_Line's own header doesn't define Geom_Curve, but
    Geom_Curve's full definition is still visible, transitively included).

    Uses `tu.cursor.get_children()`, not `tu.cursor.walk_preorder()` - same
    performance reasoning as discover_entities(): every class this generator's
    base-chain walk ever looks up is, like discover_entities' own targets, a
    direct child of the translation unit (a #include textually inserts the
    included file's top-level declarations at that point - it doesn't nest them
    under anything), so the full recursive walk was doing a great deal of
    unnecessary work descending into function bodies and nested expressions."""
    for cursor in tu.cursor.get_children():
        if cursor.kind not in (cindex.CursorKind.CLASS_DECL, cindex.CursorKind.STRUCT_DECL):
            continue
        if cursor.spelling == class_name and cursor.is_definition():
            return cursor
    return None


def base_chain(tu: cindex.TranslationUnit, class_cursor: cindex.Cursor) -> list[str]:
    """Direct parent first, then grandparent, etc. Stops after Standard_Transient
    (inclusive) or when no further base is found. OCCT's class hierarchies are
    single-inheritance throughout the modules this generator targets - only the
    first CXX_BASE_SPECIFIER is followed (see codegen/README.md's "Known gaps" for
    multiple inheritance)."""
    chain: list[str] = []
    current = class_cursor
    while True:
        bases = [c for c in current.get_children() if c.kind == cindex.CursorKind.CXX_BASE_SPECIFIER]
        if not bases:
            break
        base_name = strip_type(bases[0].type.spelling)
        chain.append(base_name)
        if base_name == TRANSIENT_ROOT:
            break
        next_cursor = find_class_definition(tu, base_name)
        if next_cursor is None:
            break
        current = next_cursor
    return chain


def has_explicit_destructor(class_cursor: cindex.Cursor) -> bool:
    return any(c.kind == cindex.CursorKind.DESTRUCTOR for c in class_cursor.get_children())


def owns_resource_unsafely(tu: cindex.TranslationUnit, class_cursor: cindex.Cursor, chain: list[str]) -> bool:
    """True if class_cursor or any class in its ancestor chain declares an explicit
    destructor - found the hard way (see DESIGN.md's "Copying an algorithm/builder
    class segfaults"): BRepAlgoAPI_Cut (via BRepAlgoAPI_BuilderAlgo) owns a raw
    pointer member (BOPAlgo_PPaveFiller) freed in ~BRepAlgoAPI_BuilderAlgo(), with no
    user-declared copy constructor - the implicitly-generated one shallow-copies the
    pointer, so returning such a type *by value* from a constructor lambda (sol2
    moving/copying it into the usertype's own storage, on top of the guaranteed-elided
    return itself) double-frees it once both copies are eventually destroyed. Not
    Standard_Transient-derived, so the Handle(T) heap-allocation path doesn't
    apply here (this generator's own opencascade::handle<T> emission requires being
    Transient-derived to interoperate with the rest of OCCT's Handle-based API) -
    heap-allocate via std::unique_ptr<T> instead (see emit_lambda), which never
    copies/moves T itself at all, regardless of whether T is *actually* safe to copy.
    A class with no explicit destructor anywhere in its chain (gp_Pnt, TopoDS_Shape,
    ...) is assumed trivially/safely copyable and keeps the plain by-value path."""
    if has_explicit_destructor(class_cursor):
        return True
    for base_name in chain:
        base_cursor = find_class_definition(tu, base_name)
        if base_cursor is not None and has_explicit_destructor(base_cursor):
            return True
    return False


def collect_class_callables(target: cindex.Cursor, class_name: str) -> list[Callable]:
    """Returns target's public constructors/methods (static and instance) as
    unfiltered Callables - target must already be the class's own CLASS_DECL/
    STRUCT_DECL definition cursor (see find_class_definition). A static method (e.g.
    BRepTools::Write) is marked is_static=True and rendered identically to a
    namespace free function - see collect_namespace_callables/codegen/README.md."""
    callables: list[Callable] = []
    for child in target.get_children():
        if child.access_specifier != cindex.AccessSpecifier.PUBLIC:
            continue

        if child.kind == cindex.CursorKind.CONSTRUCTOR:
            kind, cpp_name, return_spelling, is_const, is_static = "constructor", class_name, None, False, False
        elif child.kind == cindex.CursorKind.CXX_METHOD:
            if child.spelling.startswith("operator"):
                # See codegen/README.md's "Operator overloads and friend free
                # functions" - only a member operator whose *token* has a real Lua
                # metamethod slot (BINARY_OPERATOR_META/UNARY_OPERATOR_META) is
                # ever emitted; everything else (operator!=, compound assignment,
                # increment/decrement, conversion operators, ...) is silently
                # skipped here exactly like an unrecognized method would be -
                # there is no Lua-side slot for it to occupy, not a gap to close.
                token = child.spelling[len("operator"):].strip()
                args = list(child.get_arguments())
                if not args and token in UNARY_OPERATOR_META:
                    callables.append(Callable(
                        class_name, "operator", child.spelling, [], child.result_type.spelling,
                        child.is_const_method(), is_static=False,
                        meta_function=UNARY_OPERATOR_META[token], operator_token=token,
                    ))
                elif len(args) == 1 and token in BINARY_OPERATOR_META:
                    arg = args[0]
                    p = Param(arg.type.spelling, arg.spelling or "other", arg.type.get_canonical().spelling,
                              has_default=param_has_default(arg))
                    callables.append(Callable(
                        class_name, "operator", child.spelling, [p], child.result_type.spelling,
                        child.is_const_method(), is_static=False,
                        meta_function=BINARY_OPERATOR_META[token], operator_token=token,
                    ))
                continue
            kind, cpp_name, return_spelling, is_const, is_static = (
                "method", child.spelling, child.result_type.spelling,
                child.is_const_method(), child.is_static_method(),
            )
        else:
            continue

        params = [Param(arg.type.spelling, arg.spelling or f"arg{i}", arg.type.get_canonical().spelling,
                         has_default=param_has_default(arg))
                  for i, arg in enumerate(child.get_arguments())]
        callables.append(Callable(class_name, kind, cpp_name, params, return_spelling, is_const, is_static))
    return callables


def collect_friend_functions(class_cursor: cindex.Cursor, ns_name: str) -> list[Callable]:
    """Free functions declared `friend` inside a class body - a real C++ idiom
    (ADOL-C's own adtl::adouble uses it throughout for its math functions - sin,
    cos, ... - and its reversed-operand arithmetic/comparison operators) with
    *namespace* scope for the function itself despite being lexically nested
    inside the class (see codegen/README.md's "Operator overloads and friend free
    functions") - collect_class_callables' own get_children() walk never finds
    these (a FRIEND_DECL wraps the actual FUNCTION_DECL as its own child, not a
    CXX_METHOD), so this is a separate, dedicated walk. Only *non-operator* friend
    functions are collected here (sin/cos/... and similar) - a friend operator
    (e.g. `friend adouble operator+(double, const adouble&)`) is deliberately
    *not* picked up this way; process_module() instead synthesizes the reversed-
    operand sibling for a matching member operator directly (see its own comment),
    since that also lets it decide arity/kind() acceptance the normal way rather
    than needing this function to duplicate that logic for a second cursor shape.

    Returned as is_static=True Callables with class_name=ns_name (the *enclosing
    namespace*, not the class the friend declaration happens to be lexically
    nested in) - render_module already renders a static Callable as a plain
    qualified call (ns_name::cpp_name(args)), which is exactly correct here: C++
    itself gives a friend function namespace scope, so e.g. `adtl::sin(x)` is a
    real, valid call regardless of `sin` being declared inside `adouble`."""
    callables: list[Callable] = []
    for child in class_cursor.get_children():
        if child.kind != cindex.CursorKind.FRIEND_DECL:
            continue
        for inner in child.get_children():
            if inner.kind != cindex.CursorKind.FUNCTION_DECL:
                continue
            if inner.spelling.startswith("operator"):
                continue  # handled via process_module's reversed-operand synthesis instead
            params = [Param(arg.type.spelling, arg.spelling or f"arg{i}", arg.type.get_canonical().spelling,
                             has_default=param_has_default(arg))
                      for i, arg in enumerate(inner.get_arguments())]
            callables.append(Callable(ns_name, "method", inner.spelling, params,
                                       inner.result_type.spelling, is_const=False, is_static=True))
    return callables


def find_friend_operator_reversed(class_cursor: cindex.Cursor, token: str) -> bool:
    """Whether class_cursor's own body declares a *binary* friend
    `operator<token>` (two parameters) - used by process_module's reversed-
    operand synthesis to *confirm*, not assume, that a scalar-taking member
    operator's reversed-operand sibling (e.g. `double + adouble` alongside
    `adouble::operator+(double) const`) actually exists before emitting a Lua
    binding for it. Deliberately doesn't check the friend's own parameter types
    match exactly (self is always one of them, structurally, or it wouldn't be
    a meaningful operator for this class at all) - just that a two-argument
    friend by that name exists at all, which is enough to tell "OCCT/ADOL-C
    really defines this direction" apart from "it doesn't, and self OP other
    only ever works one way" (see process_module's own comment - gp_XYZ's
    operator/ is exactly the case that doesn't)."""
    for child in class_cursor.get_children():
        if child.kind != cindex.CursorKind.FRIEND_DECL:
            continue
        for inner in child.get_children():
            if inner.kind == cindex.CursorKind.FUNCTION_DECL and inner.spelling == f"operator{token}" \
                    and len(list(inner.get_arguments())) == 2:
                return True
    return False


def emit_lambda(c: Callable, kinds: list[str], ad_branch: bool, ctor_mode: str, helper_namespace: str) -> str:
    param_decls = []
    call_args = []
    for p, k in zip(c.params, kinds):
        if ad_branch and k == "real":
            param_decls.append(f"sol::object {p.name}")
            call_args.append(f"{helper_namespace}::to_real({p.name})")
        elif k == "array1":
            elem = array_element_type(p.canonical, ARRAY1_PATTERN)
            param_decls.append(f"std::vector<{elem}> const& {p.name}")
            # A real-element array1 (e.g. TColStd_Array1OfReal) needs
            # to_real_array1, not to_array1<double> - see its own comment in
            # src/collections.hpp for why: the Lua-side vector is always
            # std::vector<double> regardless of build, but the OCCT array's own
            # element type is Standard_Real, a distinct C++ type from double under
            # GRUNK_OCCT_WITH_ADOLC.
            fn = "to_real_array1" if elem in REAL_ELEMENT_SPELLINGS else "to_array1"
            call_args.append(f"{helper_namespace}::{fn}({p.name})")
        elif k == "array2":
            elem = array_element_type(p.canonical, ARRAY2_PATTERN)
            param_decls.append(f"std::vector<{elem}> const& {p.name}_flat, Standard_Integer {p.name}_rows, Standard_Integer {p.name}_cols")
            fn = "to_real_array2" if elem in REAL_ELEMENT_SPELLINGS else "to_array2"
            call_args.append(f"{helper_namespace}::{fn}({p.name}_flat, {p.name}_rows, {p.name}_cols)")
        else:
            param_decls.append(f"{p.spelling} {p.name}")
            call_args.append(p.name)

    if c.kind == "constructor":
        args_str = ", ".join(call_args)
        if ctor_mode == "handle":
            # Standard_Transient-derived types are never value-constructed by
            # convention (OCCT's own idiom is always heap-allocate + refcount via
            # Handle(T)) - heap-allocate here and let occt_sol_traits.hpp's
            # sol::unique_usertype_traits<opencascade::handle<T>> bridge the result
            # to Lua as if it were a T instance (see codegen/README.md's
            # Standard_Transient/Handle(T) section).
            return (f"[]({', '.join(param_decls)}) -> opencascade::handle<{c.class_name}> "
                    f"{{ return opencascade::handle<{c.class_name}>(new {c.class_name}({args_str})); }}")
        if ctor_mode == "unique_ptr":
            # Not Transient-derived, but owns a resource unsafely under copy/move
            # (see DESIGN.md's "Copying an algorithm/builder class segfaults" -
            # owns_resource_unsafely()) - heap-allocate via std::unique_ptr<T>, which
            # sol2 accepts directly as usertype-constructor storage and never
            # copies/moves T itself, regardless of whether that's actually safe.
            return (f"[]({', '.join(param_decls)}) {{ return std::make_unique<{c.class_name}>({args_str}); }}")
        return f"[]({', '.join(param_decls)}) {{ return {c.class_name}({args_str}); }}"

    if c.is_static:
        # Static class method or namespace free function - no "self" (see
        # codegen/README.md's "Static-only 'namespace classes'"). Always
        # fully-qualified (::) - correct whether class_name is a genuine C++ class
        # (BRepTools::Write) or a namespace (TopoDS::Face).
        all_decls = ", ".join(param_decls)
        call_expr = f"{c.class_name}::{c.cpp_name}({', '.join(call_args)})"
    else:
        self_decl = f"{c.class_name} const& self" if c.is_const else f"{c.class_name}& self"
        all_decls = ", ".join([self_decl] + param_decls)
        call_expr = f"self.{c.cpp_name}({', '.join(call_args)})"
    ret = (c.return_spelling or "void").strip()
    if ret == "void":
        return f"[]({all_decls}) {{ {call_expr}; }}"
    if "&" in ret and HANDLE_PATTERN.match(strip_type(ret)):
        # A Handle(X)-returning method/constructor that returns it by reference
        # (e.g. GC_MakeSegment::Value() const -> const Handle(Geom_TrimmedCurve)&)
        # is emitted as a by-value opencascade::handle<X> return instead of the raw
        # reference - found the hard way (GC_MakeSegment/GC_MakeCircle): pushing a
        # bare Handle<X> onto the Lua stack by value works fine
        # everywhere else in this project (every constructor's own Handle(X)
        # return), but pushing one *by reference* hits a sol2 code path
        # (unqualified_pusher::push_keyed's set_undefined_methods_on) that tries
        # to instantiate default operator</operator== comparisons for the
        # underlying OCCT type X itself - most OCCT geometry classes (including
        # Geom_TrimmedCurve/Geom_Circle) don't declare either, so this fails to
        # compile. A Handle is just a cheap-to-copy smart pointer (incrementing a
        # refcount, O(1)), so returning by value instead is always safe and
        # sidesteps the whole issue - unlike a plain (non-Handle) registered
        # type's own const-reference return (e.g. Geom_BezierCurve::Pole()'s
        # `const gp_Pnt&`), which does *not* hit this and is left untouched.
        ret = strip_type(ret)
    return f"[]({all_decls}) -> {ret} {{ return {call_expr}; }}"


def emit_operator_lambda(c: Callable) -> str:
    """Emits a lambda for an operator Callable (c.kind == "operator" - see
    collect_class_callables' own operator handling and codegen/README.md's
    "Operator overloads and friend free functions"). Never needs the
    Standard_Real AD dual-emission emit_lambda applies to ordinary methods/
    constructors - an operator is only ever emitted for the class *being*
    registered here (grunk-adolc's own adtl::adouble, today), never for
    Standard_Real itself, so its own parameter (if any) is never itself
    "real"-kind by construction (see emit_callable_expr's own dispatch)."""
    ret = (c.return_spelling or "").strip()
    if not c.params:
        # Unary (e.g. operator-() const) - self is the only operand.
        return f"[]({c.class_name} const& self) -> {ret} {{ return {c.operator_token}self; }}"
    other_decl = f"{c.params[0].spelling} {c.params[0].name}"
    if c.is_static:
        # Reversed-operand sibling (see process_module's own synthesis) - `other`
        # is the *first* Lua-facing argument, self the second: `other OP self`,
        # e.g. `double + adouble` via the real friend operator+(double, const
        # adouble&) - relies on C++ operator resolution finding it, not on this
        # generator having parsed that friend declaration itself (see
        # collect_friend_functions' own comment on why friend operators aren't
        # collected that way at all).
        return (f"[]({other_decl}, {c.class_name} const& self) -> {ret} "
                f"{{ return {c.params[0].name} {c.operator_token} self; }}")
    # Normal member form: self OP other.
    return (f"[]({c.class_name} const& self, {other_decl}) -> {ret} "
            f"{{ return self {c.operator_token} {c.params[0].name}; }}")


def ambiguity_safe_overload_order(
    overloads: list[tuple[Callable, list[str]]],
) -> list[tuple[Callable, list[str]]]:
    """Reorders a same-name overload set so sol2 tries the most concretely-typed
    signature first, for each argument count.

    Found the hard way (see DESIGN.md's "sol2 overload-resolution ambiguity under
    GRUNK_OCCT_WITH_ADOLC"): under the AD build, every Standard_Real parameter's
    own emitted C++ type is `sol::object`, not `double` - a deliberate, necessary
    part of the AD dual-emission pattern (see emit_lambda/occt_ad_support.hpp),
    but `sol::object` also matches *any* Lua value, including a completely
    unrelated registered usertype (a gp_Ax2, say). sol2's own overload resolution
    (both `.add_constructors(...)` and `sol::overload(...)` for regular methods)
    tries candidates in the order they were registered and commits to the first
    one whose parameter count and per-parameter type checks succeed - and since
    `sol::object` never fails that check, an all-real overload (e.g.
    `MakeSphere(Standard_Real R, Standard_Real angle1, Standard_Real angle2)`,
    emitted as three `sol::object` parameters) can silently shadow a
    same-arity, genuinely-more-specific one (`MakeSphere(gp_Ax2 const&,
    Standard_Real R, Standard_Real angle)`) that was only ever meant to apply
    when the first argument really is an axis. libclang's own cursor-walk order
    (= declaration order in the OCCT header) is what originally decided
    registration order here, with no awareness of this hazard at all - OCCT
    itself, unsurprisingly, tends to declare the "just numbers" overload before
    the "positioned by an axis/point" ones.

    The fix: within each argument-count group (sol2 already dispatches on count
    first, so different counts never compete), sort so a signature with *fewer*
    "real" kinds - i.e. more parameters sol2 can actually type-check against a
    concrete usertype, not a real-turned-sol::object stand-in - is tried before
    one with more. A stable sort, so signatures that tie (same arity, same real
    count) keep their original relative order rather than shuffling for no
    reason. Harmless, not just safe, for the plain (non-AD) build too - there,
    Standard_Real params are genuinely `double`, sol2 can already disambiguate
    every one of these cases by real type-checking regardless of order - applying
    the same reordering there anyway keeps one single ordering rule for both
    build variants, rather than a second, conditional code path that only ever
    reorders under GRUNK_OCCT_WITH_ADOLC.
    """
    return sorted(overloads, key=lambda ck: (len(ck[1]), ck[1].count("real")))


def emit_callable_expr(c: Callable, kinds: list[str], ctor_mode: str, helper_namespace: str) -> str:
    if c.kind == "operator":
        return emit_operator_lambda(c)
    has_real = "real" in kinds
    plain = emit_lambda(c, kinds, ad_branch=False, ctor_mode=ctor_mode, helper_namespace=helper_namespace)
    if not has_real:
        return plain
    ad_form = emit_lambda(c, kinds, ad_branch=True, ctor_mode=ctor_mode, helper_namespace=helper_namespace)
    return f"\n#if defined({AD_MACRO})\n            {ad_form}\n#else\n            {plain}\n#endif\n        "


@dataclass
class ModuleResult:
    name: str
    headers: list[str]
    extra_includes: list[str] = field(default_factory=list)
    accepted_ctors: dict[str, list[tuple[Callable, list[str]]]] = field(default_factory=dict)
    accepted_methods: dict[str, dict[str, list[tuple[Callable, list[str]]]]] = field(default_factory=dict)
    rejected: list[tuple[Callable, str]] = field(default_factory=list)
    blacklisted: list[Callable] = field(default_factory=list)
    is_transient: dict[str, bool] = field(default_factory=dict)
    ctor_mode: dict[str, str] = field(default_factory=dict)  # "value" | "unique_ptr" | "handle"
    bases: dict[str, list[str]] = field(default_factory=dict)
    entity_kind: dict[str, str] = field(default_factory=dict)  # "class" | "enum" | "namespace"
    entity_names: list[str] = field(default_factory=list)  # discovery order - replaces the old
    # one-name-per-header assumption (Path(h).stem for h in headers); a single header can now
    # contribute zero, one, or several entities (see discover_entities).
    enum_values: dict[str, list[str]] = field(default_factory=dict)  # enum name -> its enumerator names
    # Dedup key set for the main acceptance loop below - see its own comment on
    # why the same signature can genuinely be discovered twice (ADOL-C's own
    # friend-declared-and-separately-namespace-defined functions).
    seen_signatures: set[tuple] = field(default_factory=set)
    # class/enum name -> its enclosing namespace, only for an entity actually
    # found nested one level inside one (see discover_entities' own namespace-
    # recursion note) - e.g. "adouble" -> "adtl". render_module emits a `using
    # ns::Name;` for each of these, because every other place in this generator
    # (emit_lambda, register_type<...>, etc.) emits the *bare* entity name as a
    # C++ type - correct for every OCCT class (none of them are namespaced), but
    # not otherwise valid C++ for one that is; `using` makes the bare name resolve
    # without having to qualify it everywhere the bare name is used as a type.
    namespace_of: dict[str, str] = field(default_factory=dict)


def process_module(module: dict, cache: dict[str, ParsedHeader], registered_types: set[str], registered_enums: set[str]) -> ModuleResult:
    """registered_types/registered_enums are the sets of every class/enum name
    across ALL modules in this run (not just this module's own headers) - a type
    registered by module A is a valid parameter/return type for a callable in
    module B. Computed by main() from the same `cache` this function reads (see
    parse_headers()) before any module is actually processed - see
    codegen/README.md's "Type registration spans the whole config".

    module["headers"] is already fully expanded (glob patterns resolved to literal
    filenames - see expand_headers()) and each header may define more than one
    entity - see discover_entities(). `cache` must already hold a ParsedHeader for
    every one of them (main() calls parse_headers() up front for exactly this
    reason - each header is parsed/discovered once, not once per module that
    happens to reference it and again here).

    module["extra_includes"] (optional) is a *different* concept: extra #include
    lines the generated .cpp needs for
    a type it uses only by reference/pointer (so this module's own headers only
    forward-declare it, e.g. BRepBndLib.hxx's `class TopoDS_Shape;`) but never
    registers itself - sol2's own usertype binding needs the *complete* type
    definition in this translation unit regardless of whether some other module's
    .cpp already has it. Not a source of new registrable classes - see
    codegen/README.md's "extra_includes" section."""
    headers = module["headers"]
    blacklist = set(module.get("blacklist", []))
    glob_matched = module.get("_glob_matched_headers", set())

    result = ModuleResult(name=module["name"], headers=headers, extra_includes=module.get("extra_includes", []))

    for header in headers:
        tu, entities = cache[header].tu, cache[header].entities
        if not entities:
            if header in glob_matched:
                # Expected, occasional glob noise (see expand_headers' own
                # docstring - e.g. gp_*.hxx matching gp_Vec2f.hxx, a bare typedef
                # with no class/struct/enum of its own) - skip quietly rather than
                # failing the whole run. A *literal*, hand-picked header with
                # nothing in it is still a hard error below - almost certainly a
                # typo, not something to silently ignore.
                continue
            raise RuntimeError(f"no class/struct/enum/namespace definitions found in {header}")

        # A header can reopen the same namespace more than once - not hypothetical:
        # ADOL-C's own adtl.h has one `namespace adtl { ... }` block for its
        # (forward-declared) free functions/class and a *second* one further down
        # for their real `inline` definitions. Grouped by name up front so the
        # entity loop below processes each unique namespace name exactly once,
        # using *every* one of its cursors together (collect_namespace_callables
        # itself dedupes by (name, param types) across them - see its own
        # comment) - without this, the second occurrence would be silently
        # skipped (losing whatever's only declared there) instead of merged.
        namespace_cursors_by_name: dict[str, list[cindex.Cursor]] = {}
        for entity in entities:
            if entity.kind == "namespace":
                namespace_cursors_by_name.setdefault(entity.name, []).append(entity.cursor)
        processed_namespaces: set[str] = set()

        for entity in entities:
            name = entity.name
            if name in blacklist:
                continue  # a bare ClassName/EnumName in the blacklist skips it entirely
            if entity.kind == "namespace":
                if name in processed_namespaces:
                    continue  # already handled via its first occurrence, above
                processed_namespaces.add(name)
            result.entity_kind[name] = entity.kind
            result.entity_names.append(name)

            if entity.kind == "enum":
                result.enum_values[name] = enum_constant_names(entity.cursor)
                continue  # no callables - see render_module's own enum handling

            if entity.kind == "class":
                chain = base_chain(tu, entity.cursor)
                result.bases[name] = chain
                result.is_transient[name] = TRANSIENT_ROOT in chain
                if result.is_transient[name]:
                    result.ctor_mode[name] = "handle"
                elif owns_resource_unsafely(tu, entity.cursor, chain):
                    result.ctor_mode[name] = "unique_ptr"
                else:
                    result.ctor_mode[name] = "value"
                callables = collect_class_callables(entity.cursor, name)

                # Friend free functions (see collect_friend_functions' own doc
                # comment) have namespace, not class, scope - collected under the
                # class's enclosing namespace name if it has one (e.g. adtl::sin
                # for adtl::adouble's own friend sin), or the class's own name
                # otherwise (matches every OCCT class this generator has targeted
                # so far, none of which need this at all - friend functions are
                # only actually found once something targets a header that uses
                # this idiom, e.g. ADOL-C's own adtl.h).
                parent = entity.cursor.semantic_parent
                friend_ns = parent.spelling if parent and parent.kind == cindex.CursorKind.NAMESPACE else name
                callables += collect_friend_functions(entity.cursor, friend_ns)
                if parent and parent.kind == cindex.CursorKind.NAMESPACE:
                    result.namespace_of[name] = friend_ns

                # Reversed-operand synthesis for a binary operator whose one
                # parameter is a plain literal (e.g. `adouble::operator+(double)
                # const`), not the class's own type: C++ operator overload
                # resolution requires the *class* to appear as one of the two
                # operands (adouble::operator+ can't be a member of `double`), so
                # the "double + adouble" direction is only ever reachable via a
                # separate friend/free operator (see codegen/README.md's "Operator
                # overloads and friend free functions").
                #
                # Confirmed to exist via find_friend_operator_reversed() before
                # synthesizing anything - NOT assumed just because the member form
                # exists. Found the hard way, regenerating this project's own
                # existing OCCT config: ADOL-C's adtl::adouble (a pure arithmetic
                # type) really does define all four reversed forms (+, -, *, /)
                # alongside every scalar-taking member operator, but OCCT's own
                # geometric types don't follow that uniformly - gp_XYZ has
                # operator/(Standard_Real) const (scale down) with no reversed
                # friend at all (dividing a scalar *by* a vector isn't a defined
                # operation), while gp_Mat's own operator* does have one (scalar *
                # matrix is standard). Assuming the reversed form always exists
                # alongside the member one compiled cleanly for adtl::adouble but
                # broke gp_XYZ's own division - a real, loud compile error, exactly
                # as expected for a wrong assumption, but not something to leave
                # sitting as a landmine for the next new module/operator either.
                #
                # is_static=True marks the synthesized sibling for
                # emit_operator_lambda (self is the *second* operand, not the
                # first) - a normal member operator Callable is never itself
                # is_static, so this can't collide with one.
                reversed_callables = []
                for c in callables:
                    if c.kind != "operator" or c.is_static or len(c.params) != 1:
                        continue
                    other_kind = c.params[0].kind(registered_types, registered_enums)
                    if other_kind not in ("passthrough", "real") or c.params[0].base == name:
                        continue
                    if not find_friend_operator_reversed(entity.cursor, c.operator_token):
                        continue
                    reversed_callables.append(Callable(
                        name, "operator", c.cpp_name, [c.params[0]], c.return_spelling, c.is_const,
                        is_static=True, meta_function=c.meta_function, operator_token=c.operator_token,
                    ))
                callables += reversed_callables
            else:
                # Namespace (e.g. TopoDS.hxx's `namespace TopoDS { ... }` - see
                # codegen/README.md's "Static-only 'namespace classes'"). No base
                # chain/Transient concept applies. namespace_cursors_by_name[name]
                # (computed above) already has every cursor for this name across
                # this header - reopened within one header (adtl.h) or not.
                # Reopened across *multiple headers* in the same module still
                # isn't handled (would need the same grouping one level up, across
                # the outer `for header in headers:` loop - not needed by anything
                # targeted so far).
                callables = collect_namespace_callables(namespace_cursors_by_name[name], name)

            result.accepted_methods.setdefault(name, {})

            for c in callables:
                # c.class_name, not the outer `name`: identical for every
                # "ordinary" callable (collect_class_callables/
                # collect_namespace_callables always set it to the entity being
                # processed), but a friend function's own class_name is its
                # *enclosing namespace* instead (see collect_friend_functions'
                # own doc comment) - using c.class_name here, not name, is what
                # actually routes it there rather than under the class it was
                # lexically found nested inside.
                result.accepted_methods.setdefault(c.class_name, {})

                symbol = f"{c.class_name}::{c.cpp_name}"
                if symbol in blacklist:
                    result.blacklisted.append(c)
                    continue

                arities = c.accepted_arities(registered_types, registered_enums)
                if not arities:
                    result.rejected.append((c, c.rejection_reason(registered_types, registered_enums)))
                    continue

                # One (Callable, kinds) entry per reachable arity - see
                # accepted_arities()'s own doc comment (default-argument
                # expansion): a shorter kinds list here means emit_lambda emits a
                # shorter parameter list, dropping trailing arguments this call
                # simply never passes and letting C++ apply their real defaults.
                full_kinds = [p.kind(registered_types, registered_enums) for p in c.params]
                for i in arities:
                    kinds = full_kinds[:i]
                    # A real signature can be discovered more than once via two
                    # genuinely different AST paths - not hypothetical: ADOL-C's
                    # own `sin`/`cos`/... are declared once as a `friend` inside
                    # adouble's own class body (found via collect_friend_functions)
                    # *and* separately defined again as a plain namespace-level
                    # function (found via collect_namespace_callables, since the
                    # real `inline` body sits directly in `namespace adtl { ... }`,
                    # not nested in any class) - two different Callable objects,
                    # from two different collection functions, for what is C++-wise
                    # the exact same overload. Deduplicated here, at the one point
                    # every acceptance path converges, rather than trying to make
                    # each collection function aware of what the others might also
                    # find - the same signature tuple (spelling per parameter,
                    # truncated to this arity) can never legitimately appear twice
                    # for one cpp_name/meta_function under one class_name.
                    sig = tuple(p.spelling for p in c.params[:i])
                    if c.kind == "constructor":
                        dedup_key = (name, "__ctor__", sig)
                        if dedup_key in result.seen_signatures:
                            continue
                        result.seen_signatures.add(dedup_key)
                        result.accepted_ctors.setdefault(name, []).append((c, kinds))
                    elif c.kind == "operator":
                        # Grouped by meta_function, not cpp_name - every operator
                        # sharing one meta_function (e.g. the member "self + double"
                        # and "self + adouble" forms, plus the reversed "double +
                        # self" sibling) becomes overloads of the *same*
                        # sol::meta_function slot, never a named method - see
                        # render_module's own "__meta_" key handling.
                        dedup_key = (c.class_name, f"__meta_{c.meta_function}", c.is_static, sig)
                        if dedup_key in result.seen_signatures:
                            continue
                        result.seen_signatures.add(dedup_key)
                        result.accepted_methods[c.class_name].setdefault(f"__meta_{c.meta_function}", []).append((c, kinds))
                    else:
                        dedup_key = (c.class_name, c.cpp_name, sig)
                        if dedup_key in result.seen_signatures:
                            continue
                        result.seen_signatures.add(dedup_key)
                        result.accepted_methods[c.class_name].setdefault(c.cpp_name, []).append((c, kinds))

    return result


def render_module(result: ModuleResult, helper_namespace: str) -> tuple[str, str]:
    """Returns (header_source, cpp_source)."""
    header_source = GENERATED_BANNER + f"""
#pragma once

#include <grunk/dynamic.hpp>

void register_{result.name}(grunk::plugin_namespace& ns);
"""

    includes = "\n".join(f"#include <{h}>" for h in result.headers + result.extra_includes)
    any_transient = any(result.is_transient.values())
    any_array = any(
        k in ("array1", "array2")
        for overloads in result.accepted_ctors.values()
        for _, kinds in overloads
        for k in kinds
    ) or any(
        k in ("array1", "array2")
        for methods in result.accepted_methods.values()
        for overloads in methods.values()
        for _, kinds in overloads
        for k in kinds
    )
    # Whether this module actually emitted any Standard_Real AD dual-emission
    # code (<helper_namespace>::to_real calls - see emit_lambda) - never true for
    # a module with no genuine Standard_Real parameter anywhere (e.g. grunk-adolc's
    # own adtl module, which only ever sees bare "double", a different Param.kind()
    # - see PASSTHROUGH_TYPES). occt_ad_support.hpp is grunk-occt-specific (its
    # own #include path below is relative to *this* repo's src/ directory) and
    # must not be pulled into an unrelated project's own generated code that never
    # needed it in the first place.
    any_real = any(
        k == "real"
        for overloads in result.accepted_ctors.values()
        for _, kinds in overloads
        for k in kinds
    ) or any(
        k == "real"
        for methods in result.accepted_methods.values()
        for overloads in methods.values()
        for _, kinds in overloads
        for k in kinds
    )
    all_class_names = result.entity_names
    registered_here = set(all_class_names)

    free_function_parts = []  # static class methods + namespace free functions with no naming collision
    extra_member_functions: dict[str, list[str]] = {}  # class_name -> extra ".add_member_function(...)" lines

    for class_name in all_class_names:
        ctor_mode = result.ctor_mode.get(class_name, "value")

        for method_name, overloads in list(result.accepted_methods.get(class_name, {}).items()):
            if method_name.startswith("__meta_"):
                continue  # operators always go through the instance-method loop below, never this one
            if not overloads[0][0].is_static:
                continue
            overloads = ambiguity_safe_overload_order(overloads)
            if len(overloads) == 1:
                c, kinds = overloads[0]
                fun_expr = emit_callable_expr(c, kinds, ctor_mode, helper_namespace)
            else:
                fun_expr = "sol::overload(\n        " + ",\n        ".join(
                    emit_callable_expr(c, kinds, ctor_mode, helper_namespace) for c, kinds in overloads
                ) + "\n    )"

            # A *genuine* C++ namespace's own free functions (result.entity_kind
            # == "namespace") get registered under their own bare name where
            # possible, not prefixed - "adtl.sin", not "adtl.adtl_sin" - since
            # nothing stops them from keeping their own name directly under the
            # plugin namespace table, exactly mirroring their real C++ nesting.
            # BUT the collision check below always has to test the *prefixed*
            # name regardless of whether the enclosing scope is a genuine
            # namespace or one of OCCT's "namespace classes" (a plain C++ class
            # with only static members, e.g. the older `class TopoDS { public:
            # static ... };` form) - OCCT's own naming convention (TopoDS::Face
            # downcasting to TopoDS_Face) creates the exact same collision
            # either way, and newer OCCT (found via this project's own OCCT
            # version) actually declares TopoDS as a real `namespace`, not a
            # class, while still colliding with TopoDS_Face by that same
            # convention - checking only the bare name here would silently miss
            # it and register a plain occt.Face(shape) function instead of
            # attaching occt.TopoDS_Face.DownCast(shape), the same regression a
            # namespace-vs-class libclang cursor-kind change on OCCT's side
            # (not this generator's) can reintroduce on any future OCCT bump.
            composed_name = f"{class_name}_{method_name}"
            is_namespace = result.entity_kind.get(class_name) == "namespace"
            if composed_name in registered_here:
                # Naming collision (e.g. TopoDS::Face(shape) composes to the same
                # name as the registered TopoDS_Face class itself - a real case, not
                # hypothetical: every TopoDS namespace downcast helper collides with
                # its own destination leaf type this way). Rather than clobbering the
                # type's own occt.TopoDS_Face table with a plain function, attach it
                # as that type's own .DownCast(shape) member instead - mirrors Step
                # 3's Handle(T)::DownCast naming (src/geom/geom_extras.hpp) for a
                # consistent Lua-side idiom regardless of which OCCT downcast
                # mechanism (Handle-based vs. TopoDS-namespace-based) is underneath.
                # See codegen/README.md's "Static-only 'namespace classes'".
                extra_member_functions.setdefault(composed_name, []).append(
                    f'        .add_member_function("DownCast", {fun_expr})'
                )
            else:
                final_name = method_name if is_namespace else composed_name
                free_function_parts.append(f'    ns.register_function("{final_name}", {fun_expr});')

    body_parts = []
    for class_name in all_class_names:
        entity_kind = result.entity_kind.get(class_name)
        if entity_kind == "namespace":
            continue  # no register_type for a genuine namespace - nothing else to do
        if entity_kind == "enum":
            # A plain C-style enum (see codegen/README.md's "Enum support") -
            # sol2's own new_enum(...) is a non-const sol::table method, but
            # plugin_namespace::table() returns `sol::table const&` - constructing
            # a fresh (cheap, reference-semantics) sol::table copy from it sidesteps
            # that constness mismatch without needing a new grunk API. No explicit
            # enum-type template argument needed: this sol2 overload deduces it
            # from the value arguments themselves. Each enumerator's own bare C++
            # name doubles as its Lua-side string key (e.g.
            # occt.GeomAbs_Shape.GeomAbs_C0).
            enumerator_pairs = ",\n            ".join(
                f'"{value}", {value}' for value in result.enum_values.get(class_name, [])
            )
            body_parts.append(
                f'    sol::table(ns.table()).new_enum("{class_name}",\n            {enumerator_pairs}\n        );'
            )
            continue

        ctor_mode = result.ctor_mode.get(class_name, "value")
        instance_methods = {
            method_name: ambiguity_safe_overload_order(overloads)
            for method_name, overloads in result.accepted_methods.get(class_name, {}).items()
            # An operator group's own overloads mix is_static=False (normal member
            # forms: self OP double, self OP self) and is_static=True (the
            # reversed-operand sibling: double OP self) Callables - is_static there
            # only ever meant "which operand is self" (see emit_operator_lambda),
            # never "register this as a free function", so every __meta_-keyed
            # group always belongs here regardless of what its first overload's own
            # is_static happens to be.
            if method_name.startswith("__meta_") or not overloads[0][0].is_static
        }

        lines = [f'    ns.register_type<{class_name}, sol::automagic_flags::none>("{class_name}")']

        ctors = ambiguity_safe_overload_order(result.accepted_ctors.get(class_name, []))
        if ctors:
            ctor_exprs = [emit_callable_expr(c, kinds, ctor_mode, helper_namespace) for c, kinds in ctors]
            lines.append("        .add_constructors(\n            " + ",\n            ".join(ctor_exprs) + "\n        )")

        for method_name, overloads in instance_methods.items():
            if len(overloads) == 1:
                c, kinds = overloads[0]
                fun_expr = emit_callable_expr(c, kinds, ctor_mode, helper_namespace)
            else:
                fun_expr = "sol::overload(\n            " + ",\n            ".join(
                    emit_callable_expr(c, kinds, ctor_mode, helper_namespace) for c, kinds in overloads
                ) + "\n        )"
            if method_name.startswith("__meta_"):
                # An operator group (see process_module's own "__meta_" keying) -
                # a real sol2/Lua metamethod slot, not a named method: the key
                # here must be the sol::meta_function value itself, never a
                # quoted string, or sol2 would register it as an ordinary method
                # literally named e.g. "addition" instead of wiring up `+`.
                key_expr = f"sol::meta_function::{method_name[len('__meta_'):]}"
            else:
                key_expr = f'"{method_name}"'
            lines.append(f'        .add_member_function({key_expr}, {fun_expr})')

        lines.extend(extra_member_functions.get(class_name, []))

        # Standard_Transient/Handle(T) support, also used for plain value-type
        # inheritance (e.g. TopoDS_Face : TopoDS_Shape): register the full ancestor
        # chain so Lua's colon-call dispatch (and
        # argument-position upcasting) can find a method/accept a value via a base
        # class not registered on the derived type itself.
        bases = result.bases.get(class_name, [])
        if bases:
            lines.append(f"        .add_bases<{', '.join(bases)}>()")

        body_parts.append("\n".join(lines) + ";")

    extra_includes = '\n#include "../src/occt_sol_traits.hpp"' if any_transient else ""
    extra_includes += '\n#include "../src/collections.hpp"\n#include <vector>' if any_array else ""
    extra_includes += '\n#include "../src/occt_ad_support.hpp"' if any_real else ""

    # A `using` for every entity actually found nested inside a namespace (see
    # discover_entities' own namespace-recursion note and ModuleResult.namespace_of's
    # own comment) - every other place in this file emits the entity's *bare* name
    # as a C++ type (register_type<Name>, lambda parameter types, Name(args)
    # construction, ...), which is correct only because no OCCT class this generator
    # has ever targeted is namespaced; for one that is (ADOL-C's own adtl::adouble),
    # this makes the bare name resolve without threading a second, qualified name
    # through every one of those emission sites instead.
    using_decls = "\n".join(
        f"using {ns}::{name};" for name, ns in sorted(result.namespace_of.items())
    )

    cpp_source = GENERATED_BANNER + f"""
#include "{result.name}.hpp"

{includes}

#include <memory>
{extra_includes}

{using_decls}

void register_{result.name}(grunk::plugin_namespace& ns)
{{
{chr(10).join(body_parts)}
{chr(10).join(free_function_parts)}
}}
"""
    return header_source, cpp_source


def compute_translation_units(config: dict, module_names: list[str]) -> list[str]:
    """Computes CMakeLists.txt's translation-unit grouping from the optional
    top-level `translation_units:` config key, returning the manifest lines to write
    to generated/translation_units.txt (see that file's own header comment and
    DESIGN.md's "Handle(X) return type requires being in the same translation unit
    as X" for why this exists at all: a Handle(X)-returning module's generated code
    only compiles correctly when X's own register_type<X, ...>() call is compiled
    in the *same* translation unit).

    `translation_units.mode` ("separate", the default, or "unity") controls what
    happens to a module that isn't explicitly listed in any `translation_units.groups`
    entry: "separate" gives it its own singleton translation unit (finest granularity -
    keeps a single compiler invocation's memory/time bounded, recommended default);
    "unity" merges every module (and CMakeLists.txt's own hand-written entry point,
    src/occt_plugin.cpp) into exactly one translation unit, matching this project's
    original, simplest-possible design - a single line "*" is returned in that case,
    which CMakeLists.txt recognizes as "ignore everything below and do the old
    unity-everything build."

    `translation_units.groups` (a list of lists of module names) is consulted
    regardless of `mode`: each inner list names a set of modules that must compile
    together, in one translation unit - use this for a confirmed cross-module
    Handle(X) dependency (see the `gc`/`geom` entry in config.yml). A module named in
    a group is never also treated as its own singleton, even under `mode: separate`.
    """
    tu_config = config.get("translation_units", {})
    mode = tu_config.get("mode", "separate")
    groups: list[list[str]] = tu_config.get("groups", [])

    if mode not in ("separate", "unity"):
        raise ValueError(f'translation_units.mode must be "separate" or "unity", got {mode!r}')

    module_name_set = set(module_names)
    seen: set[str] = set()
    for group in groups:
        for name in group:
            if name not in module_name_set:
                raise ValueError(f'translation_units.groups references unknown module "{name}"')
            if name in seen:
                raise ValueError(f'translation_units.groups lists module "{name}" in more than one group')
            seen.add(name)

    if mode == "unity":
        return ["*"]

    lines: list[str] = []
    emitted: set[str] = set()
    for name in module_names:
        if name in emitted:
            continue
        group = next((g for g in groups if name in g), [name])
        lines.append(" ".join(group))
        emitted.update(group)
    return lines


def build_clang_args(include_dir: Path) -> list[str]:
    args = ["-x", "c++", "-std=c++17", f"-I{include_dir}"]
    args += [f"-isystem{d}" for d in system_cxx_include_dirs()]
    return args


def add_arguments(parser: argparse.ArgumentParser) -> None:
    """Declares this generator's CLI flags on parser - factored out of main() so
    grunk.cli's `codegen` subcommand can reuse the exact same flag definitions
    rather than redeclaring them a second time (see run())."""
    parser.add_argument("--config", type=Path, default=Path(__file__).parent / "config.yml")
    parser.add_argument("--include-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)


def run(args: argparse.Namespace) -> int:
    """The generator's actual body, taking already-parsed args - see
    add_arguments(). Split from main() so grunk.cli's `codegen` subcommand can
    invoke it directly against its own subparser's Namespace."""
    config = yaml.safe_load(args.config.read_text())
    args.output_dir.mkdir(parents=True, exist_ok=True)
    clang_args = build_clang_args(args.include_dir)
    # The C++ namespace (see src/collections.hpp / occt_ad_support.hpp-style
    # helpers) that this plugin's own to_real/to_array1/to_array2 conversion
    # helpers live in - config-driven since this generator is now shared across
    # plugins, each with its own such namespace (grunk-occt: occt_plugin,
    # grunk-adolc: adolc_plugin) - see emit_lambda.
    helper_namespace = config["helper_namespace"]

    for module in config["modules"]:
        module["headers"], module["_glob_matched_headers"] = expand_headers(
            module["headers"], args.include_dir, module.get("exclude_headers", [])
        )

    # Every header across the whole config is parsed/discovered exactly once here
    # (see parse_headers()'s own docstring) - both registered_types/registered_enums
    # (computed from this same cache, below) and each module's own process_module()
    # call read from it rather than re-parsing.
    cache: dict[str, ParsedHeader] = {}
    for module in config["modules"]:
        parse_headers(module["headers"], args.include_dir, clang_args, cache)

    registered_types = {e.name for p in cache.values() for e in p.entities if e.kind == "class"}
    registered_enums = {e.name for p in cache.values() for e in p.entities if e.kind == "enum"}

    for module in config["modules"]:
        result = process_module(module, cache, registered_types, registered_enums)
        header_source, cpp_source = render_module(result, helper_namespace)
        (args.output_dir / f"{result.name}.hpp").write_text(header_source)
        (args.output_dir / f"{result.name}.cpp").write_text(cpp_source)

        n_accepted = sum(len(v) for v in result.accepted_ctors.values()) + sum(
            len(o) for m in result.accepted_methods.values() for o in m.values()
        )
        print(f"[{result.name}] {n_accepted} accepted, {len(result.rejected)} rejected, "
              f"{len(result.blacklisted)} blacklisted", file=sys.stderr)
        for c, reason in result.rejected:
            print(f"  rejected {c.class_name}::{c.cpp_name}: {reason}", file=sys.stderr)
        for c in result.blacklisted:
            print(f"  blacklisted {c.class_name}::{c.cpp_name}", file=sys.stderr)

    # See CMakeLists.txt's own comment on reading this file: one line per translation
    # unit, each a space-separated list of module names (generated/<name>.cpp) to
    # compile together, or a single line "*" meaning "one translation unit for
    # everything, including src/occt_plugin.cpp" (mode: unity - see
    # compute_translation_units's own docstring).
    module_names = [module["name"] for module in config["modules"]]
    tu_lines = compute_translation_units(config, module_names)
    (args.output_dir / "translation_units.txt").write_text("\n".join(tu_lines) + "\n")

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_arguments(parser)
    return run(parser.parse_args())


if __name__ == "__main__":
    sys.exit(main())
