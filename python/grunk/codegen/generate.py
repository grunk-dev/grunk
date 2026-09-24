#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

"""grunk's shared code generator: parses a set of C++ headers (via libclang) driven
by a declarative per-plugin config (codegen/config.yml) and emits grunk plugin
registration code (generated/<module>.cpp) targeting grunk's register_type/
add_constructors/add_member_function API. Shared by every native grunk plugin that
wraps a third-party C++ library this way.

This file itself has no knowledge of, or reference to, any specific third-party
library. A module's `headers`/`blacklist`/`exclude_headers`/`extra_includes` and the
handful of genuinely universal C++ idioms this generator understands out of the box
(plain value construction, heap-allocation for a class with an explicit destructor,
passthrough of a registered class/enum/C++ fundamental) need no further
configuration at all - see CodeGenerator's own docstring for everything else (a
smart-pointer wrapper, a fixed-size array/container type, a type with more than one
possible C++ representation, or any other library-specific idiom), which a plugin
teaches this generator via an optional `code_generator:` config key naming a Python
file that subclasses CodeGenerator.

See python/grunk/codegen/README.md for the full design (what's accepted, what's
rejected and why, known gaps) - deliberately narrow in scope: constructors/instance
methods only, and only a small allowlist of "simple" parameter/return types can be
bound without a CodeGenerator subclass. Anything else is silently skipped and
reported in the summary printed at the end of a run - review that output when
adding a new module, since it's the primary way to discover what needs a
hand-written escape hatch or a CodeGenerator override.

A module's `headers` list accepts glob patterns (e.g. "gp_*.hxx") as well as literal
filenames, expanded against --include-dir - see expand_headers(). Each matched
header is scanned for every class/struct/enum/namespace *directly* declared in it (not
just one matching the filename stem, and not anything nested or only transitively
included) - see discover_entities().
"""

from __future__ import annotations

import argparse
import importlib.util
import inspect
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
    *after* its header has already parsed successfully, so it can't help here).
    Only ever applies to headers a glob actually matched (a literal, hand-picked
    header listed in exclude_headers as well as headers would be a config
    contradiction, not a real use case, so this isn't guarded against explicitly).

    Returns (headers, glob_matched) - glob_matched is the subset that came from a
    wildcard rather than being named explicitly, which process_module uses to
    decide how strict to be about a header defining nothing this generator cares
    about (see its own docstring): a hand-picked literal header with zero
    class/struct/enum/namespace definitions is almost certainly a typo/mistake and
    stays a hard error, but a glob is *expected* to occasionally sweep in a header
    that doesn't declare a real type at all, which should be skipped quietly
    rather than failing the whole run.

    Known hazard: a prefix-style glob can accidentally sweep in an unrelated
    header family with the same filename prefix - the blacklist mechanism (a bare
    ClassName) is the way to exclude something an over-eager glob picked up, same
    as excluding anything else this generator shouldn't bind."""
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


# Fundamental C++ types (not any third-party library's own typedef aliases) that
# pass straight through to Lua as plain values, with zero conversion needed in the
# emitted lambda body - a CodeGenerator subclass extends this via its own
# classify_param() for a library's own typedef aliases of these (e.g. one library
# might always spell a boolean through its own BOOL/Boolean typedef, never bare
# `bool`) - see CodeGenerator's own docstring.
CXX_FUNDAMENTAL_PASSTHROUGH_TYPES = {"double", "unsigned int", "bool", "size_t"}

# The full, closed vocabulary of C++ built-in arithmetic/bool/char type
# spellings, as libclang canonicalizes them - used only to recognize when an
# unfamiliar type's own *canonical* (typedef-resolved) spelling reveals it's
# secretly one of these, even though its own name looks like a plausible
# class (a different library's own "Standard_Real"-style typedef for a
# fundamental) - see Param.kind()'s own fallback for why a mutable reference
# to one of these must stay rejected even though a mutable reference to a
# genuine (typedef-free) class does not.
CXX_FUNDAMENTAL_CANONICAL_SPELLINGS = {
    "bool", "char", "signed char", "unsigned char", "wchar_t", "char16_t", "char32_t",
    "short", "unsigned short", "int", "unsigned int", "long", "unsigned long",
    "long long", "unsigned long long", "float", "double", "long double", "size_t",
}

# A plausible bare C++ type name (an unqualified or ::-namespace-qualified
# identifier - "gp_Pnt", "occt::gp_Pnt") - as opposed to a pointer ("T *"), a
# template instantiation ("Handle<T>"), or anything else with a shape a hook
# above should have already recognized (or rejected) on its own. Used only as
# Param.kind()'s own last-resort fallback: see its own docstring for why a
# name this generator run doesn't itself recognize still gets treated as an
# "object" rather than rejected.
BARE_TYPE_NAME = re.compile(r"[A-Za-z_]\w*(::[A-Za-z_]\w*)*")

# Operator overloads (see python/grunk/codegen/README.md's "Operator overloads and
# friend free functions"): C++ operator token -> sol2 meta_function name.
# Deliberately a small subset of what C++ allows overloading, not "every operator
# token" - each entry here corresponds to a real Lua metamethod slot; several
# common C++ operators have *no* Lua equivalent at all and are silently skipped
# (never added to either map below, falling through collect_class_callables' own
# "not a recognized operator" branch exactly like an unsupported one):
# operator!=/>/>= (Lua derives these from __eq/__lt/__le itself - see lua.org's
# manual on operator inheritance for comparisons - registering them natively would
# be redundant, not wrong, but pointless surface area), compound assignment
# (+=, -=, ... - Lua has no assignment-operator metamethod, `x += y` isn't valid
# Lua syntax at all), increment/decrement (same reason), plain operator=
# (assignment has its own grunk-level semantics via sol2's usertype machinery, not
# a metamethod), and conversion operators (operator double() etc. - would need a
# different mechanism entirely, not attempted here). Genuinely library-agnostic -
# every C++ type overloading one of these tokens gets the same Lua metamethod,
# regardless of which library defines it.
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

# The comment banner written at the top of every generated .hpp/.cpp - carries no
# copyright/license content of its own on purpose, since that's specific to each
# plugin's own authorship, not something this shared, library-agnostic generator
# should assume (an SPDX header naming grunk's own author would be simply wrong
# atop a different plugin's generated code). A plugin wanting one (virtually all
# of them, for REUSE-compliance) supplies its own verbatim text via config.yml's
# optional `generated_banner` key (see run()) - this is only the fallback used
# when that key is absent.
DEFAULT_GENERATED_BANNER = """\
// AUTO-GENERATED by grunk.codegen from codegen/config.yml - do not edit by hand,
// your changes will be overwritten the next time this module is regenerated.
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
    # synthetic Param a Callable's own rejection_reason/accepted_arities build to
    # check a return type; a CodeGenerator subclass's own classify_param() can use
    # this (self.name being empty signals "this is a return-type check, not a
    # genuine parameter") the same way the built-in kinds already treat it.
    canonical: str = ""
    # Whether the real C++ declaration gives this parameter a default value (e.g.
    # `SomeType Copy = false`) - see param_has_default() for how this is actually
    # detected via libclang, and Callable.accepted_arities() for what it unlocks
    # (default-argument expansion). Only ever True for a genuine parameter
    # (populated by collect_class_callables/collect_namespace_callables), never
    # for the synthetic Param a Callable's own rejection_reason/accepted_arities
    # construct to check a return type.
    has_default: bool = False

    @property
    def base(self) -> str:
        return strip_type(self.spelling)

    @property
    def is_mutable_ref(self) -> bool:
        return "&" in self.spelling and "const" not in self.spelling

    def kind(self, registered_types: set[str], registered_enums: set[str], hooks: "CodeGenerator") -> str | None:
        """None if unsupported, else a short "kind" tag driving how
        CodeGenerator.emit_callable() declares/converts this parameter. The
        built-ins here ("passthrough" for a fundamental/enum, "object" for a
        class - registered by this run or not, see below) need zero
        customization from any plugin; anything else - a smart-pointer
        wrapper, a fixed-size array/container type, a type with more than one
        possible C++ representation, or any other library-specific idiom - is
        delegated to hooks.classify_param(), which returns None (still
        unsupported) unless the plugin's own CodeGenerator subclass
        recognizes it.

        Non-const-reference ("out") parameters ARE supported for an object
        (Lua/sol2 userdata for a class already has stable, mutable identity -
        the same userdata the caller passed in gets mutated in place, exactly
        like OOP mutation works in Lua natively) but NOT for a bare
        fundamental/enum ("passthrough") - Lua has no mutable-number-by-
        reference concept, there is no userdata to mutate in place for a bare
        value. A plugin's own classify_param() is free to make the same
        distinction, or a different one, for its own custom kinds.

        A class need not be one of *this* run's own registered_types to
        classify as "object": sol2 does not require a class to have been
        registered (by this run, or at all, yet) to bind a function taking or
        returning it by value or reference - pushing/popping a value of a
        type sol2 has never seen still works, via its own automatic userdata
        boxing (confirmed empirically against sol2 itself, not assumed - a
        function bound before its parameter type was ever registered, and
        even before it was registered *at all*, still worked correctly once
        called with a value of that type). registered_types/registered_enums
        are still checked first purely because a type *this* run itself
        discovered gets first refusal on its own name (and an enum
        discovered by this run gets the more conservative "passthrough"
        rather than "object" - see above); a type this run doesn't know about
        - most commonly one registered by a *different*, dependency plugin -
        still classifies as "object" as long as its spelling is a plausible
        bare type name (not a pointer, function type, or something a hook
        above already tried and rejected for a structural reason), mutable
        reference included: a name nobody recognizes might just as well be a
        fundamental hiding behind an unfamiliar typedef (a different
        library's own "Standard_Real"), and a mutable reference to *that*
        would be silently wrong the same way a bare fundamental's own
        mutable reference already is - but the *canonical* (typedef-resolved)
        spelling, when available, tells the two apart reliably (a real
        class's canonical spelling is never one of
        CXX_FUNDAMENTAL_CANONICAL_SPELLINGS), so only a mutable reference
        that canonically resolves to one of those - or one for which no
        canonical spelling is available at all, i.e. a return-type check, see
        Param.canonical's own field docstring - stays conservative and
        rejected."""
        if self.base in CXX_FUNDAMENTAL_PASSTHROUGH_TYPES:
            return None if self.is_mutable_ref else "passthrough"
        # A plain C-style enum - sol2 marshals it as a plain value with zero
        # special-casing needed in the emitted lambda body, exactly like a
        # fundamental above - reuses the "passthrough" kind rather than a new one.
        if self.base in registered_enums:
            return None if self.is_mutable_ref else "passthrough"
        if self.base in registered_types:
            return "object"
        kind = hooks.classify_param(self, registered_types, registered_enums)
        if kind is not None:
            return kind
        if not BARE_TYPE_NAME.fullmatch(self.base):
            return None
        # self.canonical (typedef-resolved) is only populated for a genuine
        # parameter, never for the synthetic Param a return-type check builds
        # (see its own field docstring). Where it *is* available, cross-check
        # it too: guards against a library's own "pointer to X" typedef
        # convention (e.g. OCCT's own BOPAlgo_PPaveFiller =
        # BOPAlgo_PaveFiller*) - self.base alone can't see through a typedef
        # to notice it's secretly a pointer, but self.canonical can (found
        # the hard way: silently pushing a raw pointer as if it were a plain
        # object is a different, riskier shape than this fallback is meant
        # to cover).
        canonical_base = strip_type(self.canonical) if self.canonical else None
        if canonical_base is not None and not BARE_TYPE_NAME.fullmatch(canonical_base):
            return None
        if self.is_mutable_ref:
            # Safe (a mutable reference to a genuine, stable-identity class -
            # same as an in-run registered class already supports) only when
            # the canonical spelling positively confirms this isn't secretly
            # a fundamental; unavailable (a return-type check) or actually
            # fundamental both stay conservative and rejected.
            if canonical_base is None or canonical_base in CXX_FUNDAMENTAL_CANONICAL_SPELLINGS:
                return None
        return "object"


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
    # handling and python/grunk/codegen/README.md's "Operator overloads and friend
    # free functions"): meta_function is the sol2 sol::meta_function member name
    # (e.g. "addition"); operator_token is the real C++ operator token (e.g. "+"),
    # used by emit_operator_lambda to build the actual C++ expression - self OP
    # other for a normal member operator, other OP self for the synthesized
    # reversed-operand sibling process_module() adds for a binary operator whose
    # only parameter is a plain literal (double) rather than the class's own type.
    meta_function: str | None = None
    operator_token: str | None = None

    def rejection_reason(self, registered_types: set[str], registered_enums: set[str], hooks: "CodeGenerator") -> str | None:
        for p in self.params:
            if p.kind(registered_types, registered_enums, hooks) is None:
                return f"unsupported parameter type '{p.spelling.strip()}'"
        if self.return_spelling is not None and self.return_spelling.strip() != "void":
            if Param(self.return_spelling, "").kind(registered_types, registered_enums, hooks) is None:
                return f"unsupported return type '{self.return_spelling.strip()}'"
        return None

    def accepted_arities(self, registered_types: set[str], registered_enums: set[str], hooks: "CodeGenerator") -> list[int]:
        """Every number of leading parameters this callable can be called with from
        Lua, in ascending order - normally just [len(self.params)] (today's
        all-or-nothing behavior, when nothing is defaulted), but more than one value
        when trailing parameters have real C++ default values: this generator
        doesn't need to know the *value* of a defaulted trailing parameter, only
        that it has one - a shorter overload can simply omit it from the emitted
        call expression and let C++ itself apply the real default at the actual
        call site (see param_has_default()). Returns [] if nothing is
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
            if Param(self.return_spelling, "").kind(registered_types, registered_enums, hooks) is None:
                return []
        n = len(self.params)
        kinds = [p.kind(registered_types, registered_enums, hooks) for p in self.params]
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
    #included from it). Each becomes its own Entity, independent of the header's
    own filename: a header may define zero, one, or several (a real case once
    `headers` accepts glob patterns - see expand_headers() - rather than each
    header being hand-picked to match exactly one wanted class).

    Only *direct* children of the translation unit, or of a namespace that is
    itself one, are considered - one level of namespace nesting is followed (see
    below), but never a second (a namespace inside a namespace) or into a class
    body at all - this is what excludes a class's own protected/private nested
    helper classes and a class-scoped member enum, without needing a separate
    filter for each. A class/struct must be a real definition (`is_definition()`,
    excludes forward declarations); an enum must additionally have a name (skips
    anonymous enums, which have no symbolic type to register). "std" is excluded
    by name even though it technically qualifies (a header using some library's
    own hashing support might reopen `namespace std { template<> struct
    hash<...> {...}; }` right there, at true TU-child scope) - harmless either way
    (render_module skips every "namespace" entity's own registration regardless,
    and a plain namespace reopen has no free FUNCTION_DECLs for
    collect_namespace_callables to find), just noise in the discovered-entity list.

    One level of namespace nesting is followed - needed for a library whose types
    live inside a real C++ namespace rather than using prefix-based naming (e.g.
    `namespace mylib { class Widget; }` rather than `mylib_Widget`) - without
    this, such a class is never discovered as a "class" entity at all, silently
    registering none of its constructors/methods/operators and leaving every
    friend function nested inside it (see collect_friend_functions) permanently
    rejected as an unsupported parameter type - the class itself never being a
    registered type. A second level of nesting (a namespace inside a namespace)
    is deliberately not followed - not needed by anything targeted so far, and
    each additional level compounds the performance cost the note below is about.

    Performance note: uses `cursor.get_children()` (direct children only, of the
    translation unit and - one level down - of a namespace within it), *not*
    `tu.cursor.walk_preorder()` (which recurses into every cursor's entire subtree
    - function bodies, nested expressions, template instantiation internals, the
    works). Since every entity this function looks for is by definition at most
    one level down already, walk_preorder()'s extra recursion buys nothing here
    but cost: for a single moderately-included header, it visits on the order of
    10^5 cursors where get_children() visits closer to 10^3 - the difference
    between this finishing in well under a second and a full regen run not
    finishing inside a 10-minute timeout at all. Found the hard way."""
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
                # header (e.g. a header directly defining `class
                # SomeTemplate<ConcreteArg> { ... };`) - its cursor.kind is an
                # ordinary CLASS_DECL and cursor.spelling is just the bare
                # template name, no argument, which would otherwise get
                # registered as if it were a plain, template-free class named
                # that - cursor.type.spelling, unlike .spelling, does include the
                # template argument, so a mismatch between the two is exactly the
                # signal a specialization is present.
                #
                # .rsplit("::", 1)[-1] first: cursor.type.spelling is fully
                # qualified (e.g. "mylib::Widget" for a class one level inside a
                # namespace - see this function's own note on following namespace
                # nesting), cursor.spelling never is - comparing the two directly,
                # unqualified, would flag *every* namespaced class as a
                # false-positive "specialization" and silently drop it.
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
    """The enumerator names of an ENUM_DECL cursor, in declaration order - see
    python/grunk/codegen/README.md's "Enum support" for how these become both the
    Lua-side key and the bare C++ value in the emitted new_enum(...) call."""
    return [c.spelling for c in enum_cursor.get_children() if c.kind == cindex.CursorKind.ENUM_CONSTANT_DECL]


@dataclass
class ParsedHeader:
    tu: cindex.TranslationUnit
    entities: list[Entity]


def parse_headers(headers: list[str], include_dir: Path, clang_args: list[str],
                   cache: dict[str, ParsedHeader]) -> None:
    """Parses each header not already in cache and discovers its entities, storing
    both under cache[header] - shared by run()'s whole-config discovery pass and
    process_module()'s own per-module pass so each header is only ever parsed
    once, regardless of how many modules reference it. Parsing (and, for a
    heavily-included header, walking its AST) is the dominant cost here - Standard
    C++ header parsing runs in the ~0.1-0.5s range per header even for a single
    pass, so avoiding a second full pass over the same header roughly halves total
    runtime once a config has any overlap between the whole-config discovery scan
    and each module's own processing (which a wildcard-expanded config, with
    modules split apart along natural sub-boundaries, will have far more of than
    a one-header-per-module design would)."""
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
# all for a bare fundamental type) - none of those three indicate a default
# value. Any *other* child kind appearing on a parameter is the default-value
# expression itself (CXX_BOOL_LITERAL_EXPR for a boolean default,
# INTEGER_LITERAL/FLOATING_LITERAL for a numeric default, DECL_REF_EXPR for an
# enum-constant default, UNEXPOSED_EXPR for a temporary-object default).
# libclang has no dedicated "has a default value" query on a parameter cursor, so
# this is the standard heuristic other libclang-based tools use for the same
# question.
NON_DEFAULT_CHILD_KINDS = {
    cindex.CursorKind.TYPE_REF,
    cindex.CursorKind.TEMPLATE_REF,
    cindex.CursorKind.NAMESPACE_REF,
}


def param_has_default(arg_cursor: cindex.Cursor) -> bool:
    return any(child.kind not in NON_DEFAULT_CHILD_KINDS for child in arg_cursor.get_children())


def collect_namespace_callables(namespace_cursors: list[cindex.Cursor], ns_name: str) -> list[Callable]:
    """Returns every free FUNCTION_DECL directly inside the given namespace
    fragment(s) as unfiltered, is_static=True Callables (see
    python/grunk/codegen/README.md's "Static-only 'namespace classes'" - a
    namespace free function and a class's static method are emitted identically:
    no self, ns_name::cpp_name(args)).

    Deduplicates by (spelling, parameter-type tuple) across every fragment - not a
    hypothetical: a header can reopen the same namespace more than once with a
    forward *declaration* in one place and the real *definition* in another - both
    are independent FUNCTION_DECL cursors for the logically same function, and
    without this, both would be collected as separate overloads, generating the
    same lambda body twice."""
    callables: list[Callable] = []
    seen: set[tuple[str, tuple[str, ...]]] = set()
    for target in namespace_cursors:
        for child in target.get_children():
            if child.kind != cindex.CursorKind.FUNCTION_DECL:
                continue
            if child.spelling.startswith("operator"):
                continue  # operators not handled yet - see python/grunk/codegen/README.md's "Known gaps"
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
    headers.

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
    """Direct parent first, then grandparent, etc., up to the most distant
    ancestor whose own definition is still visible in this translation unit
    (stops there, or when a class has no further base at all). Purely
    structural AST-walking - no notion of "where a plugin's own idiom
    considers this hierarchy to end" belongs here; see
    CodeGenerator.emit_bases() for how a plugin filters/truncates this raw
    chain for its own `.add_bases<...>()` emission. Class hierarchies this
    generator targets are assumed single-inheritance - only the first
    CXX_BASE_SPECIFIER is followed (multiple inheritance is a known gap, not
    attempted)."""
    chain: list[str] = []
    current = class_cursor
    while True:
        bases = [c for c in current.get_children() if c.kind == cindex.CursorKind.CXX_BASE_SPECIFIER]
        if not bases:
            break
        base_name = strip_type(bases[0].type.spelling)
        chain.append(base_name)
        next_cursor = find_class_definition(tu, base_name)
        if next_cursor is None:
            break
        current = next_cursor
    return chain


def has_explicit_destructor(class_cursor: cindex.Cursor) -> bool:
    return any(c.kind == cindex.CursorKind.DESTRUCTOR for c in class_cursor.get_children())


def owns_resource_unsafely(tu: cindex.TranslationUnit, class_cursor: cindex.Cursor, chain: list[str]) -> bool:
    """True if class_cursor or any class in its ancestor chain declares an explicit
    destructor - a real hazard, not hypothetical (see the consuming plugin's own
    DESIGN.md for a concrete worked example, if it has one): a class that owns a
    raw resource (e.g. a pointer freed in its own destructor) with no
    user-declared copy constructor gets an implicitly-generated one that
    shallow-copies that resource, so returning such a type *by value* from a
    constructor lambda (sol2 moving/copying it into the usertype's own storage,
    on top of the guaranteed-elided return itself) can double-free it once both
    copies are eventually destroyed. Heap-allocating via std::unique_ptr<T>
    instead (see CodeGenerator.emit_callable's own ctor_mode dispatch) sidesteps
    this entirely, since it never copies/moves T itself at all, regardless of
    whether T is *actually* safe to copy. A class with no explicit destructor
    anywhere in its chain is assumed trivially/safely copyable and keeps the
    plain by-value path."""
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
    STRUCT_DECL definition cursor (see find_class_definition). A static method is
    marked is_static=True and rendered identically to a namespace free function -
    see collect_namespace_callables/python/grunk/codegen/README.md."""
    callables: list[Callable] = []
    for child in target.get_children():
        if child.access_specifier != cindex.AccessSpecifier.PUBLIC:
            continue

        if child.kind == cindex.CursorKind.CONSTRUCTOR:
            kind, cpp_name, return_spelling, is_const, is_static = "constructor", class_name, None, False, False
        elif child.kind == cindex.CursorKind.CXX_METHOD:
            if child.spelling.startswith("operator"):
                # See python/grunk/codegen/README.md's "Operator overloads and
                # friend free functions" - only a member operator whose *token*
                # has a real Lua metamethod slot (BINARY_OPERATOR_META/
                # UNARY_OPERATOR_META) is ever emitted; everything else
                # (operator!=, compound assignment, increment/decrement,
                # conversion operators, ...) is silently skipped here exactly
                # like an unrecognized method would be - there is no Lua-side
                # slot for it to occupy, not a gap to close.
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
    (used throughout by, e.g., a tapeless-AD scalar type for its math functions -
    sin, cos, ... - and its reversed-operand arithmetic/comparison operators)
    with *namespace* scope for the function itself despite being lexically nested
    inside the class (see python/grunk/codegen/README.md's "Operator overloads
    and friend free functions") - collect_class_callables' own get_children()
    walk never finds these (a FRIEND_DECL wraps the actual FUNCTION_DECL as its
    own child, not a CXX_METHOD), so this is a separate, dedicated walk. Only
    *non-operator* friend functions are collected here (sin/cos/... and similar) -
    a friend operator (e.g. `friend Widget operator+(double, const Widget&)`) is
    deliberately *not* picked up this way; process_module() instead synthesizes
    the reversed-operand sibling for a matching member operator directly (see its
    own comment), since that also lets it decide arity/kind() acceptance the
    normal way rather than needing this function to duplicate that logic for a
    second cursor shape.

    Returned as is_static=True Callables with class_name=ns_name (the *enclosing
    namespace*, not the class the friend declaration happens to be lexically
    nested in) - render_module already renders a static Callable as a plain
    qualified call (ns_name::cpp_name(args)), which is exactly correct here: C++
    itself gives a friend function namespace scope, so e.g. `mylib::sin(x)` is a
    real, valid call regardless of `sin` being declared inside `Widget`."""
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
    operator's reversed-operand sibling (e.g. `double + Widget` alongside
    `Widget::operator+(double) const`) actually exists before emitting a Lua
    binding for it. Deliberately doesn't check the friend's own parameter types
    match exactly (self is always one of them, structurally, or it wouldn't be a
    meaningful operator for this class at all) - just that a two-argument friend
    by that name exists at all, which is enough to tell "the library really
    defines this direction" apart from "it doesn't, and self OP other only ever
    works one way" (not every type overloading a scalar-taking operator also
    defines the reversed friend form - assuming it always does is a real,
    silent-until-it-doesn't-compile mistake, found the hard way once)."""
    for child in class_cursor.get_children():
        if child.kind != cindex.CursorKind.FRIEND_DECL:
            continue
        for inner in child.get_children():
            if inner.kind == cindex.CursorKind.FUNCTION_DECL and inner.spelling == f"operator{token}" \
                    and len(list(inner.get_arguments())) == 2:
                return True
    return False


def emit_operator_lambda(c: Callable) -> str:
    """Emits a lambda for an operator Callable (c.kind == "operator"). Operators
    are always emitted plainly, with no multi-variant concept involved - a plugin
    needing that for its own operators can still override
    CodeGenerator.emit_callable() wholesale (see its own docstring)."""
    ret = (c.return_spelling or "").strip()
    if not c.params:
        # Unary (e.g. operator-() const) - self is the only operand.
        return f"[]({c.class_name} const& self) -> {ret} {{ return {c.operator_token}self; }}"
    other_decl = f"{c.params[0].spelling} {c.params[0].name}"
    if c.is_static:
        # Reversed-operand sibling (see process_module's own synthesis) - `other`
        # is the *first* Lua-facing argument, self the second: `other OP self`,
        # e.g. `double + Widget` via the real friend operator+(double, const
        # Widget&) - relies on C++ operator resolution finding it, not on this
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
    hooks: "CodeGenerator",
) -> list[tuple[Callable, list[str]]]:
    """Reorders a same-name overload set so sol2 tries the most concretely-typed
    signature first, for each argument count.

    Relevant whenever a plugin's own classify_param()/emit_callable() gives one
    of its own kinds a maximally-permissive Lua-side declaration for some build
    variant (e.g. a dynamically-typed value that matches *any* Lua argument, not
    just its own real C++ type - see CodeGenerator.emit_callable's own docstring
    on multi-variant emission) - sol2's own overload resolution (both
    `.add_constructors(...)` and `sol::overload(...)`) tries candidates in the
    order they were registered and commits to the first one whose parameter
    count and per-parameter type checks succeed, so an all-permissive overload
    can silently shadow a same-arity, genuinely-more-specific one that was only
    ever meant to apply in a narrower case. See the consuming plugin's own
    DESIGN.md for a concrete story, if it has one.

    The fix: within each argument-count group (sol2 already dispatches on count
    first, so different counts never compete), sort so a signature with *fewer*
    hooks.OVERLOAD_AMBIGUITY_KINDS-kind parameters - i.e. more parameters sol2
    can actually type-check against something concrete - is tried before one
    with more. A stable sort, so signatures that tie (same arity, same count)
    keep their original relative order rather than shuffling for no reason.
    Harmless, not just safe, whenever OVERLOAD_AMBIGUITY_KINDS is empty (the
    default) or a given signature has none of those kinds at all - a no-op
    reordering that costs nothing when it isn't needed."""
    return sorted(overloads, key=lambda ck: (len(ck[1]), sum(1 for k in ck[1] if k in hooks.OVERLOAD_AMBIGUITY_KINDS)))


class CodeGenerator:
    """Owns every piece of C++ text/type-classification decision that's specific to
    the third-party library a plugin's config.yml targets - this base class itself
    has no knowledge of, or reference to, any such library. Every method here has
    a working default implementation covering the handful of genuinely universal
    C++ idioms this generator understands out of the box (plain value
    construction, heap-allocation via std::unique_ptr for a class with an
    explicit destructor anywhere in its base chain, and passthrough of a
    registered class/enum/C++ fundamental) - a plugin whose headers never need
    anything beyond that (e.g. a library whose public API only ever produces
    bare fundamentals and its own registrable classes) needs no config.yml
    `code_generator:` key and no subclass at all.

    A plugin whose library uses idioms beyond that - a reference-counted
    smart-pointer wrapper, a fixed-size array/container type, a type with more
    than one possible C++ representation selected by a build-time macro, or
    anything else - subclasses this class in its own Python file (referenced
    from config.yml's `code_generator:` key, a path relative to config.yml's own
    directory) and overrides only what it needs, calling `super()` for
    everything it doesn't customize - the same pattern grunk's own pre-Lua-
    rewrite code generator used (see git tag v0.3.2's `python/codegen.py`
    `CodeGenerator` class), revived here for the current libclang-based design.

    See python/grunk/codegen/README.md for a fully worked example using an
    invented, deliberately non-real library, and the consuming plugin repo's own
    codegen/hooks.py (if it has one) for a real-world one."""

    # Kind names (as returned by classify_param(), or the built-in "passthrough"/
    # "object") that make a same-arity overload set ambiguous under sol2's own
    # first-match overload resolution - see ambiguity_safe_overload_order()'s own
    # docstring. Empty by default: only relevant to a plugin whose own
    # emit_callable() override gives some kind a maximally-permissive Lua-side
    # declaration in at least one of its variants.
    OVERLOAD_AMBIGUITY_KINDS: set[str] = set()

    # Kind names (as returned by classify_param(), or the built-in "passthrough")
    # that process_module()'s reversed-operand-operator synthesis considers "a
    # bare scalar operand worth synthesizing `other OP self` for" - see
    # process_module()'s own comment. Defaults to the built-in "passthrough" kind
    # alone, which is what a library's own reversed-operand operators (e.g.
    # `5 + widget` alongside `widget + 5`) need with zero customization, as long
    # as the scalar operand is a plain C++ fundamental.
    REVERSIBLE_OPERAND_KINDS: frozenset[str] = frozenset({"passthrough"})

    def classify_param(self, param: Param, registered_types: set[str], registered_enums: set[str]) -> str | None:
        """Called by Param.kind() only after its own built-in checks (fundamental/
        enum passthrough, already-registered class) fail to classify param.base.
        Returns any kind name this subclass chooses (a plain string, not drawn
        from a fixed enum this generator defines) for emit_param()/
        emit_constructor() to later recognize by that same name, or None to
        leave param unclassified here - NOT necessarily rejected outright:
        Param.kind() still tries its own unregistered-class fallback afterward
        (see its own docstring) before actually giving up, so returning None
        only means "this isn't one of my own library's special idioms," not
        "this parameter is unsupported." Receives the full Param (not just its
        spelling) so a subclass can use param.canonical (typedef-resolved
        spelling - needed to see through a library's own container/template
        typedefs) and param.is_mutable_ref (a mutable reference to a value with
        no stable Lua-side identity to mutate in place, e.g. a bare number or a
        by-value container, is usually not supported - see Param.kind's own
        docstring for the built-in kinds' own version of this rule). Default:
        always None."""
        return None

    def classify_ctor_mode(self, class_name: str, base_chain: list[str]) -> str | None:
        """Called by process_module() before the two permanent built-in modes
        ("unique_ptr" if the class or an ancestor declares an explicit
        destructor, else "value") are considered. base_chain is the class's
        full ancestor chain (direct parent first - see base_chain()'s own
        docstring), letting a subclass recognize a named base class, a marker
        attribute, a naming convention, or anything else its own library uses
        to signal a non-default lifetime policy (e.g. always heap-allocated
        and reference-counted via a smart-pointer wrapper - see
        docs/writing_plugins.rst's "A structural example" for a fully worked
        one). Must never itself return "value" or "unique_ptr" - those two
        names are reserved for the built-ins; emit_callable() never calls
        emit_constructor() for either. Default: always None (falls through to
        the two built-ins)."""
        return None

    def emit_bases(self, class_name: str, chain: list[str]) -> list[str]:
        """The base classes (direct parent first) to register via this class's
        `.add_bases<...>()` call, so Lua's colon-call dispatch and argument-
        position upcasting can find a method/accept a value via a base class
        not registered on the derived type itself. chain is base_chain()'s
        raw, untruncated result for this class - every ancestor whose own
        definition is visible in this translation unit, however far that
        goes. Override to filter or truncate it, e.g. when a library's class
        hierarchy has deeper ancestors that aren't meaningful to expose via
        Lua, or aren't themselves complete/registered types (see
        docs/writing_plugins.rst's "A structural example"). Default: chain
        unchanged."""
        return chain

    def emit_param(self, param: Param, kind: str) -> tuple[str, str] | None:
        """Declaration text and call-argument text for one parameter of kind
        (Param.kind()'s own return value - a built-in kind as well as a
        plugin-chosen one is passed here, so a subclass can, if it ever needs
        to, customize how an ordinary "passthrough"/"object" parameter is
        declared too) - e.g. ("std::vector<double> const& xs",
        "my_ns::to_vector(xs)"). A single logical parameter may already expand
        into a multi-argument C++ declaration/call by simply embedding commas in
        these two strings. Return None to fall back to the plain built-in
        declaration (f"{param.spelling} {param.name}") and call argument
        (param.name) - this is what makes the built-in "passthrough"/"object"
        kinds need no override at all. Default: always None."""
        return None

    def emit_constructor(self, class_name: str, ctor_mode: str, param_decls: list[str], args_str: str) -> str:
        """Called only when ctor_mode is neither "value" nor "unique_ptr" (see
        classify_ctor_mode) - must return the complete constructor lambda source
        text. Default: raises NotImplementedError, deliberately loud - reaching
        this with no override means classify_ctor_mode() returned a mode this
        subclass never taught emit_constructor() how to actually build."""
        raise NotImplementedError(
            f"classify_ctor_mode() returned {ctor_mode!r} for {class_name!r}, but this "
            f"CodeGenerator subclass never overrode emit_constructor() to handle it"
        )

    def emit_return_type(self, return_spelling: str) -> str | None:
        """Lets a subclass rewrite the emitted C++ return-type text for a
        non-void return (e.g. stripping a reference off a returned
        smart-pointer-wrapper type that a sol2 binding can't push by reference -
        see the consuming plugin's own DESIGN.md for why, if it has this need at
        all). Return None to use return_spelling as-is. Default: always None."""
        return None

    def emit_callable(self, c: Callable, kinds: list[str], ctor_mode: str) -> str:
        """The top-level entry point: renders one complete C++ expression for a
        constructor, method, static/namespace function, or operator, given its
        already-classified parameter kinds and constructor mode. The default
        implementation dispatches operators to emit_operator_lambda() (always
        generic, never needs overriding) and otherwise builds one ordinary
        lambda by declaring/converting each parameter via emit_param() (falling
        back to the plain built-in for any kind it returns None for),
        dispatching ctor_mode to the two built-ins or emit_constructor(), and
        applying emit_return_type().

        Override this method wholesale for anything needing more than one
        textual variant of the same callable - e.g. a type with two possible
        C++ representations selected by a build-time preprocessor macro. There
        is nothing "variant"-shaped anywhere in this base class or in the rest
        of this file: call this same method (via super()) as many times as
        needed, with this subclass's own private instance state changed between
        calls (read back by this subclass's own emit_param() override), and
        combine the returned strings however this subclass likes - a plain
        Python composition pattern, not a generator-provided primitive. See
        python/grunk/codegen/README.md for a fully worked example."""
        if c.kind == "operator":
            return emit_operator_lambda(c)

        param_decls: list[str] = []
        call_args: list[str] = []
        for p, k in zip(c.params, kinds):
            emitted = self.emit_param(p, k)
            if emitted is None:
                param_decls.append(f"{p.spelling} {p.name}")
                call_args.append(p.name)
            else:
                decl, arg = emitted
                param_decls.append(decl)
                call_args.append(arg)

        if c.kind == "constructor":
            args_str = ", ".join(call_args)
            if ctor_mode == "value":
                return f"[]({', '.join(param_decls)}) {{ return {c.class_name}({args_str}); }}"
            if ctor_mode == "unique_ptr":
                # sol2 accepts a std::unique_ptr<T> directly as usertype-
                # constructor storage and never copies/moves T itself,
                # regardless of whether that's actually safe (see
                # owns_resource_unsafely()) - standard-library, not any
                # third-party library's own idiom, so this stays a permanent
                # built-in alongside "value".
                return f"[]({', '.join(param_decls)}) {{ return std::make_unique<{c.class_name}>({args_str}); }}"
            return self.emit_constructor(c.class_name, ctor_mode, param_decls, args_str)

        if c.is_static:
            # Static class method or namespace free function - no "self".
            # Always fully-qualified (::) - correct whether class_name is a
            # genuine C++ class or a namespace.
            all_decls = ", ".join(param_decls)
            call_expr = f"{c.class_name}::{c.cpp_name}({', '.join(call_args)})"
        else:
            self_decl = f"{c.class_name} const& self" if c.is_const else f"{c.class_name}& self"
            all_decls = ", ".join([self_decl] + param_decls)
            call_expr = f"self.{c.cpp_name}({', '.join(call_args)})"
        ret = (c.return_spelling or "void").strip()
        if ret == "void":
            return f"[]({all_decls}) {{ {call_expr}; }}"
        override = self.emit_return_type(ret)
        if override is not None:
            ret = override
        return f"[]({all_decls}) -> {ret} {{ return {call_expr}; }}"


@dataclass
class ModuleResult:
    name: str
    headers: list[str]
    extra_includes: list[str] = field(default_factory=list)
    accepted_ctors: dict[str, list[tuple[Callable, list[str]]]] = field(default_factory=dict)
    accepted_methods: dict[str, dict[str, list[tuple[Callable, list[str]]]]] = field(default_factory=dict)
    rejected: list[tuple[Callable, str]] = field(default_factory=list)
    blacklisted: list[Callable] = field(default_factory=list)
    ctor_mode: dict[str, str] = field(default_factory=dict)  # "value" | "unique_ptr" | a plugin-chosen mode
    bases: dict[str, list[str]] = field(default_factory=dict)
    entity_kind: dict[str, str] = field(default_factory=dict)  # "class" | "enum" | "namespace"
    entity_names: list[str] = field(default_factory=list)  # discovery order - replaces the old
    # one-name-per-header assumption (Path(h).stem for h in headers); a single header can now
    # contribute zero, one, or several entities (see discover_entities).
    enum_values: dict[str, list[str]] = field(default_factory=dict)  # enum name -> its enumerator names
    # Dedup key set for the main acceptance loop below - see its own comment on
    # why the same signature can genuinely be discovered twice (a class whose
    # friend-declared functions are also separately visible as plain namespace
    # functions).
    seen_signatures: set[tuple] = field(default_factory=set)
    # class/enum name -> its enclosing namespace, only for an entity actually
    # found nested one level inside one (see discover_entities' own namespace-
    # recursion note). render_module emits a `using ns::Name;` for each of
    # these, because every other place in this generator (emit_callable,
    # register_type<...>, etc.) emits the *bare* entity name as a C++ type -
    # correct for an unnamespaced class, but not otherwise valid C++ for one
    # that is; `using` makes the bare name resolve without having to qualify it
    # everywhere the bare name is used as a type.
    namespace_of: dict[str, str] = field(default_factory=dict)


def process_module(module: dict, cache: dict[str, ParsedHeader], registered_types: set[str],
                    registered_enums: set[str], hooks: CodeGenerator) -> ModuleResult:
    """registered_types/registered_enums are the sets of every class/enum name
    across ALL modules in this run (not just this module's own headers) - a type
    registered by module A is a valid parameter/return type for a callable in
    module B. Computed by run() from the same `cache` this function reads (see
    parse_headers()) before any module is actually processed - see
    python/grunk/codegen/README.md's "Type registration spans the whole config".

    module["headers"] is already fully expanded (glob patterns resolved to literal
    filenames - see expand_headers()) and each header may define more than one
    entity - see discover_entities(). `cache` must already hold a ParsedHeader for
    every one of them (run() calls parse_headers() up front for exactly this
    reason - each header is parsed/discovered once, not once per module that
    happens to reference it and again here).

    module["extra_includes"] (optional) is a *different* concept: extra #include
    lines the generated .cpp needs for a type it uses only by reference/pointer
    (so this module's own headers only forward-declare it) but never registers
    itself - sol2's own usertype binding needs the *complete* type definition in
    this translation unit regardless of whether some other module's .cpp already
    has it. Not a source of new registrable classes - see
    python/grunk/codegen/README.md's "extra_includes" section."""
    headers = module["headers"]
    blacklist = set(module.get("blacklist", []))
    glob_matched = module.get("_glob_matched_headers", set())

    result = ModuleResult(name=module["name"], headers=headers, extra_includes=module.get("extra_includes", []))

    for header in headers:
        tu, entities = cache[header].tu, cache[header].entities
        if not entities:
            if header in glob_matched:
                # Expected, occasional glob noise (see expand_headers' own
                # docstring) - skip quietly rather than failing the whole run. A
                # *literal*, hand-picked header with nothing in it is still a
                # hard error below - almost certainly a typo, not something to
                # silently ignore.
                continue
            raise RuntimeError(f"no class/struct/enum/namespace definitions found in {header}")

        # A header can reopen the same namespace more than once - not
        # hypothetical (e.g. a forward-declared free-function block up top and
        # the real `inline` definitions in a second block further down).
        # Grouped by name up front so the entity loop below processes each
        # unique namespace name exactly once, using *every* one of its cursors
        # together (collect_namespace_callables itself dedupes by (name, param
        # types) across them - see its own comment) - without this, the second
        # occurrence would be silently skipped (losing whatever's only declared
        # there) instead of merged.
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
                mode = hooks.classify_ctor_mode(name, chain)
                if mode is None:
                    mode = "unique_ptr" if owns_resource_unsafely(tu, entity.cursor, chain) else "value"
                result.ctor_mode[name] = mode
                callables = collect_class_callables(entity.cursor, name)

                # Friend free functions (see collect_friend_functions' own doc
                # comment) have namespace, not class, scope - collected under
                # the class's enclosing namespace name if it has one, or the
                # class's own name otherwise (friend functions are only
                # actually found once something targets a header that uses
                # this idiom).
                parent = entity.cursor.semantic_parent
                friend_ns = parent.spelling if parent and parent.kind == cindex.CursorKind.NAMESPACE else name
                callables += collect_friend_functions(entity.cursor, friend_ns)
                if parent and parent.kind == cindex.CursorKind.NAMESPACE:
                    result.namespace_of[name] = friend_ns

                # Reversed-operand synthesis for a binary operator whose one
                # parameter is a plain literal (e.g. `Widget::operator+(double)
                # const`), not the class's own type: C++ operator overload
                # resolution requires the *class* to appear as one of the two
                # operands (Widget::operator+ can't be a member of `double`),
                # so the "double + Widget" direction is only ever reachable via
                # a separate friend/free operator (see
                # python/grunk/codegen/README.md's "Operator overloads and
                # friend free functions").
                #
                # Confirmed to exist via find_friend_operator_reversed() before
                # synthesizing anything - NOT assumed just because the member
                # form exists (a library's own reversed-operand convention can
                # be inconsistent across its own types - assuming it's always
                # there compiled cleanly for one type and broke another, a
                # real, loud compile error, found the hard way once - not
                # something to leave sitting as a landmine for the next new
                # module/operator either).
                #
                # is_static=True marks the synthesized sibling for
                # emit_operator_lambda (self is the *second* operand, not the
                # first) - a normal member operator Callable is never itself
                # is_static, so this can't collide with one.
                reversed_callables = []
                for c in callables:
                    if c.kind != "operator" or c.is_static or len(c.params) != 1:
                        continue
                    other_kind = c.params[0].kind(registered_types, registered_enums, hooks)
                    if other_kind not in hooks.REVERSIBLE_OPERAND_KINDS or c.params[0].base == name:
                        continue
                    if not find_friend_operator_reversed(entity.cursor, c.operator_token):
                        continue
                    reversed_callables.append(Callable(
                        name, "operator", c.cpp_name, [c.params[0]], c.return_spelling, c.is_const,
                        is_static=True, meta_function=c.meta_function, operator_token=c.operator_token,
                    ))
                callables += reversed_callables
            else:
                # Namespace (see python/grunk/codegen/README.md's "Static-only
                # 'namespace classes'"). No base chain/ctor-mode concept
                # applies. namespace_cursors_by_name[name] (computed above)
                # already has every cursor for this name across this header -
                # reopened within one header or not. Reopened across *multiple
                # headers* in the same module still isn't handled (would need
                # the same grouping one level up, across the outer `for header
                # in headers:` loop - not needed by anything targeted so far).
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

                arities = c.accepted_arities(registered_types, registered_enums, hooks)
                if not arities:
                    result.rejected.append((c, c.rejection_reason(registered_types, registered_enums, hooks)))
                    continue

                # One (Callable, kinds) entry per reachable arity - see
                # accepted_arities()'s own doc comment (default-argument
                # expansion): a shorter kinds list here means emit_callable
                # emits a shorter parameter list, dropping trailing arguments
                # this call simply never passes and letting C++ apply their
                # real defaults.
                full_kinds = [p.kind(registered_types, registered_enums, hooks) for p in c.params]
                for i in arities:
                    kinds = full_kinds[:i]
                    # A real signature can be discovered more than once via two
                    # genuinely different AST paths - not hypothetical (e.g. a
                    # `friend` function declared once inside a class body,
                    # found via collect_friend_functions, and separately
                    # defined again as a plain namespace-level function, found
                    # via collect_namespace_callables) - two different Callable
                    # objects, from two different collection functions, for
                    # what is C++-wise the exact same overload. Deduplicated
                    # here, at the one point every acceptance path converges,
                    # rather than trying to make each collection function
                    # aware of what the others might also find - the same
                    # signature tuple (spelling per parameter, truncated to
                    # this arity) can never legitimately appear twice for one
                    # cpp_name/meta_function under one class_name.
                    sig = tuple(p.spelling for p in c.params[:i])
                    if c.kind == "constructor":
                        dedup_key = (name, "__ctor__", sig)
                        if dedup_key in result.seen_signatures:
                            continue
                        result.seen_signatures.add(dedup_key)
                        result.accepted_ctors.setdefault(name, []).append((c, kinds))
                    elif c.kind == "operator":
                        # Grouped by meta_function, not cpp_name - every
                        # operator sharing one meta_function (e.g. the member
                        # "self + double" and "self + Widget" forms, plus the
                        # reversed "double + self" sibling) becomes overloads
                        # of the *same* sol::meta_function slot, never a named
                        # method - see render_module's own "__meta_" key
                        # handling.
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


def render_module(result: ModuleResult, hooks: CodeGenerator,
                   includes_for_kinds: dict[str, list[str]],
                   includes_for_ctor_modes: dict[str, list[str]],
                   banner: str = DEFAULT_GENERATED_BANNER) -> tuple[str, str]:
    """Returns (header_source, cpp_source). includes_for_kinds/
    includes_for_ctor_modes are config.yml's own optional top-level keys (plain
    data, not hook methods - see config.yml's own comments) mapping a
    classify_param()/classify_ctor_mode() kind name this module actually used to
    the extra #include lines its emitted code needs (e.g. a hand-written support
    header defining the helper functions a custom emit_param() calls into).
    banner is config.yml's own optional `generated_banner` key (run() passes
    DEFAULT_GENERATED_BANNER when absent) - verbatim text written at the top of
    both emitted files, typically an SPDX header plus an AUTO-GENERATED notice;
    see DEFAULT_GENERATED_BANNER's own comment for why this generator doesn't
    assume one itself."""
    header_source = banner + f"""
#pragma once

#include <grunk/dynamic.hpp>

void register_{result.name}(grunk::plugin_namespace& ns);
"""

    includes = "\n".join(f"#include <{h}>" for h in result.headers + result.extra_includes)

    # Every distinct kind/ctor-mode name this module's *accepted* ctors/methods
    # actually used - config-driven extra #includes (below) are looked up
    # against these, generalizing the old fixed "did this module use a Handle/
    # array/AD-real parameter anywhere" boolean trio into an open-ended set of
    # plugin-chosen names.
    kinds_used: set[str] = set()
    for overloads in result.accepted_ctors.values():
        for _, kinds in overloads:
            kinds_used.update(kinds)
    for methods in result.accepted_methods.values():
        for overloads in methods.values():
            for _, kinds in overloads:
                kinds_used.update(kinds)
    ctor_modes_used = set(result.ctor_mode.values())

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
            overloads = ambiguity_safe_overload_order(overloads, hooks)
            if len(overloads) == 1:
                c, kinds = overloads[0]
                fun_expr = hooks.emit_callable(c, kinds, ctor_mode)
            else:
                fun_expr = "sol::overload(\n        " + ",\n        ".join(
                    hooks.emit_callable(c, kinds, ctor_mode) for c, kinds in overloads
                ) + "\n    )"

            # A *genuine* C++ namespace's own free functions (result.entity_kind
            # == "namespace") get registered under their own bare name where
            # possible, not prefixed - since nothing stops them from keeping
            # their own name directly under the plugin namespace table, exactly
            # mirroring their real C++ nesting. BUT the collision check below
            # always has to test the *prefixed* name regardless of whether the
            # enclosing scope is a genuine namespace or a "namespace class" (a
            # plain C++ class with only static members) - a library's own
            # naming convention can create the exact same collision either way
            # (e.g. a downcast helper composing to the same name as its own
            # destination leaf type), and a namespace-vs-class libclang
            # cursor-kind change on the library's own side (not this
            # generator's) can reintroduce this on any future header bump -
            # checking only the bare name here would silently miss it and
            # register a plain free function instead of attaching it as the
            # colliding type's own member.
            composed_name = f"{class_name}_{method_name}"
            is_namespace = result.entity_kind.get(class_name) == "namespace"
            if composed_name in registered_here:
                # Naming collision - rather than clobbering the colliding
                # type's own table with a plain function, attach it as that
                # type's own .DownCast(shape)-style member instead, for a
                # consistent Lua-side idiom regardless of which downcast
                # mechanism is underneath. See python/grunk/codegen/README.md's
                # "Static-only 'namespace classes'".
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
            # A plain C-style enum (see python/grunk/codegen/README.md's "Enum
            # support") - sol2's own new_enum(...) is a non-const sol::table
            # method, but plugin_namespace::table() returns `sol::table
            # const&` - constructing a fresh (cheap, reference-semantics)
            # sol::table copy from it sidesteps that constness mismatch
            # without needing a new grunk API. No explicit enum-type template
            # argument needed: this sol2 overload deduces it from the value
            # arguments themselves. Each enumerator's own bare C++ name
            # doubles as its Lua-side string key.
            enumerator_pairs = ",\n            ".join(
                f'"{value}", {value}' for value in result.enum_values.get(class_name, [])
            )
            body_parts.append(
                f'    sol::table(ns.table()).new_enum("{class_name}",\n            {enumerator_pairs}\n        );'
            )
            continue

        ctor_mode = result.ctor_mode.get(class_name, "value")
        instance_methods = {
            method_name: ambiguity_safe_overload_order(overloads, hooks)
            for method_name, overloads in result.accepted_methods.get(class_name, {}).items()
            # An operator group's own overloads mix is_static=False (normal
            # member forms: self OP double, self OP self) and is_static=True
            # (the reversed-operand sibling: double OP self) Callables -
            # is_static there only ever meant "which operand is self" (see
            # emit_operator_lambda), never "register this as a free function",
            # so every __meta_-keyed group always belongs here regardless of
            # what its first overload's own is_static happens to be.
            if method_name.startswith("__meta_") or not overloads[0][0].is_static
        }

        lines = [f'    ns.register_type<{class_name}, sol::automagic_flags::none>("{class_name}")']

        ctors = ambiguity_safe_overload_order(result.accepted_ctors.get(class_name, []), hooks)
        if ctors:
            ctor_exprs = [hooks.emit_callable(c, kinds, ctor_mode) for c, kinds in ctors]
            lines.append("        .add_constructors(\n            " + ",\n            ".join(ctor_exprs) + "\n        )")

        for method_name, overloads in instance_methods.items():
            if len(overloads) == 1:
                c, kinds = overloads[0]
                fun_expr = hooks.emit_callable(c, kinds, ctor_mode)
            else:
                fun_expr = "sol::overload(\n            " + ",\n            ".join(
                    hooks.emit_callable(c, kinds, ctor_mode) for c, kinds in overloads
                ) + "\n        )"
            if method_name.startswith("__meta_"):
                # An operator group (see process_module's own "__meta_"
                # keying) - a real sol2/Lua metamethod slot, not a named
                # method: the key here must be the sol::meta_function value
                # itself, never a quoted string, or sol2 would register it as
                # an ordinary method literally named e.g. "addition" instead
                # of wiring up `+`.
                key_expr = f"sol::meta_function::{method_name[len('__meta_'):]}"
            else:
                key_expr = f'"{method_name}"'
            lines.append(f'        .add_member_function({key_expr}, {fun_expr})')

        lines.extend(extra_member_functions.get(class_name, []))

        # Base-class support, also used for plain value-type inheritance:
        # register the full ancestor chain so Lua's colon-call dispatch (and
        # argument-position upcasting) can find a method/accept a value via a
        # base class not registered on the derived type itself.
        bases = hooks.emit_bases(class_name, result.bases.get(class_name, []))
        if bases:
            lines.append(f"        .add_bases<{', '.join(bases)}>()")

        body_parts.append("\n".join(lines) + ";")

    # Extra #includes this module's own emitted code needs, beyond its own
    # headers/module["extra_includes"] - config-driven (see this function's own
    # docstring), keyed by whichever classify_param()/classify_ctor_mode() kind
    # names this module actually used. ctor-mode includes before param-kind ones
    # (each group in the config's own declared order, not kinds_used/
    # ctor_modes_used's own set iteration order) for a deterministic, reviewable
    # diff regardless of hash-seed randomization. Deduplicated (dict.fromkeys,
    # preserves first-seen order) since two different kinds are free to share the
    # same include list (e.g. two different array dimensionalities both needing
    # the same collection-conversion header) without that header being emitted
    # twice.
    extra_include_lines: list[str] = []
    for mode, lines_for_mode in includes_for_ctor_modes.items():
        if mode in ctor_modes_used:
            extra_include_lines.extend(lines_for_mode)
    for kind, lines_for_kind in includes_for_kinds.items():
        if kind in kinds_used:
            extra_include_lines.extend(lines_for_kind)
    extra_include_lines = list(dict.fromkeys(extra_include_lines))
    extra_includes = ("\n" + "\n".join(f"#include {entry}" for entry in extra_include_lines)) if extra_include_lines else ""

    # A `using` for every entity actually found nested inside a namespace (see
    # discover_entities' own namespace-recursion note and
    # ModuleResult.namespace_of's own comment) - every other place in this file
    # emits the entity's *bare* name as a C++ type (register_type<Name>, lambda
    # parameter types, Name(args) construction, ...), which is correct only for
    # an unnamespaced entity; for one that is namespaced, this makes the bare
    # name resolve without threading a second, qualified name through every one
    # of those emission sites instead.
    using_decls = "\n".join(
        f"using {ns}::{name};" for name, ns in sorted(result.namespace_of.items())
    )

    cpp_source = banner + f"""
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
    to generated/translation_units.txt (see that file's own header comment - a
    module returning a by-value reference-counted-wrapper type, e.g. via
    classify_ctor_mode()'s "handle"-style modes, generally needs to be compiled in
    the same translation unit as whatever module registers the wrapped type
    itself).

    `translation_units.mode` ("separate", the default, or "unity") controls what
    happens to a module that isn't explicitly listed in any `translation_units.groups`
    entry: "separate" gives it its own singleton translation unit (finest granularity -
    keeps a single compiler invocation's memory/time bounded, recommended default);
    "unity" merges every module (and the consuming plugin's own hand-written entry
    point) into exactly one translation unit - a single line "*" is returned in
    that case, which the consuming CMakeLists.txt recognizes as "ignore everything
    below and do the old unity-everything build."

    `translation_units.groups` (a list of lists of module names) is consulted
    regardless of `mode`: each inner list names a set of modules that must compile
    together, in one translation unit - use this for a confirmed cross-module
    dependency. A module named in a group is never also treated as its own
    singleton, even under `mode: separate`.
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


def load_code_generator(config: dict, config_path: Path) -> CodeGenerator:
    """Loads the plugin's own CodeGenerator subclass, per config["code_generator"]
    (a path relative to config_path's own directory - matches every other
    path-shaped config.yml concept, needs no import-path/packaging machinery).
    Absent key -> the base CodeGenerator() itself, unmodified - the concrete
    proof a plugin with no library-specific idioms needs zero hook code at all.

    Auto-discovers the subclass by inheritance (mirroring grunk's own pre-Lua-
    rewrite code generator, git tag v0.3.2's python/codegen.py, which used the
    same inspect.getmembers()+issubclass scan) rather than requiring one fixed
    class name, erroring clearly if the referenced file defines zero or more
    than one CodeGenerator subclass. No sandboxing attempted or appropriate here
    - a plugin's own hooks file is exactly as trusted as its own hand-written
    C++ support code, same author/repo/review process; "safe" here just means a
    clear, specific error message instead of a bare traceback."""
    hooks_path_str = config.get("code_generator")
    if hooks_path_str is None:
        return CodeGenerator()
    hooks_path = (config_path.parent / hooks_path_str).resolve()
    if not hooks_path.is_file():
        raise RuntimeError(f"code_generator: {hooks_path_str!r} not found ({hooks_path})")
    spec = importlib.util.spec_from_file_location(f"_grunk_codegen_{hooks_path.stem}", hooks_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    subclasses = [
        obj for _, obj in inspect.getmembers(module)
        if inspect.isclass(obj) and issubclass(obj, CodeGenerator) and obj is not CodeGenerator
    ]
    if len(subclasses) == 0:
        raise RuntimeError(f"{hooks_path} defines no CodeGenerator subclass")
    if len(subclasses) > 1:
        raise RuntimeError(
            f"{hooks_path} defines more than one CodeGenerator subclass: "
            f"{', '.join(c.__name__ for c in subclasses)}"
        )
    return subclasses[0]()


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
    hooks = load_code_generator(config, args.config)
    includes_for_kinds = config.get("includes_for_kinds", {})
    includes_for_ctor_modes = config.get("includes_for_ctor_modes", {})
    banner = config.get("generated_banner", DEFAULT_GENERATED_BANNER)

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
        result = process_module(module, cache, registered_types, registered_enums, hooks)
        header_source, cpp_source = render_module(result, hooks, includes_for_kinds, includes_for_ctor_modes, banner)
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

    # See the consuming CMakeLists.txt's own comment on reading this file: one
    # line per translation unit, each a space-separated list of module names
    # (generated/<name>.cpp) to compile together, or a single line "*" meaning
    # "one translation unit for everything" (mode: unity - see
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
