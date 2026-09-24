# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

"""Tests for grunk.codegen.generate - the shared, library-agnostic libclang-based
code generator used by grunk-occt/grunk-adolc (see its own module docstring, and
grunk.codegen.generate.CodeGenerator's own docstring for the extension mechanism).
Deliberately free of any OCCT/ADOL-C-specific type name or idiom - those tests live
in the consuming plugin's own test suite instead (e.g. grunk-occt's own
tests/python/test_hooks.py), since they exercise that plugin's own CodeGenerator
subclass, not this shared, otherwise-agnostic module. Split into three parts:
pure-logic unit tests exercising the generic, built-in behavior (no libclang
parsing involved); a small toy CodeGenerator subclass proving the hook-machinery
plumbing itself works generically (a fake kind, a fake dual-emission macro); and
integration tests that parse a real, synthetic header via libclang end-to-end
through run(), to catch a regression in the parsing layer itself that the
pure-logic tests, by construction, cannot see."""

import argparse
from pathlib import Path

import yaml
import pytest

from grunk.codegen import generate


_hooks = generate.CodeGenerator()


# ---------------------------------------------------------------------------
# strip_type()
# ---------------------------------------------------------------------------

def test_strip_type_removes_const_and_reference():
    assert generate.strip_type("const Widget &") == "Widget"
    assert generate.strip_type("Widget") == "Widget"
    assert generate.strip_type("double") == "double"
    assert generate.strip_type("const   double  &") == "double"


# ---------------------------------------------------------------------------
# Param.kind() - the built-in kinds only (passthrough/object); anything a
# CodeGenerator subclass classifies itself belongs in that plugin's own tests.
# ---------------------------------------------------------------------------

def test_param_kind_fundamental_passthrough():
    assert generate.Param("double", "x").kind(set(), set(), _hooks) == "passthrough"
    assert generate.Param("bool &", "x").kind(set(), set(), _hooks) is None  # mutable-ref rejected


def test_param_kind_registered_enum_and_type():
    assert generate.Param("Color", "x").kind(set(), {"Color"}, _hooks) == "passthrough"
    assert generate.Param("Widget", "x").kind({"Widget"}, set(), _hooks) == "object"
    assert generate.Param("Widget &", "x").kind({"Widget"}, set(), _hooks) == "object"  # mutable "out" param IS supported


def test_param_kind_unregistered_class_still_classifies_as_object():
    # sol2 doesn't require a class to be pre-registered (by this run, or at
    # all, yet) to bind a function taking/returning it - confirmed
    # empirically against sol2 itself, see Param.kind()'s own docstring. A
    # bare, unregistered type name - most commonly one registered by a
    # *different* plugin this one depends on - is accepted, not rejected.
    assert generate.Param("Widget", "x").kind(set(), set(), _hooks) == "object"
    assert generate.Param("ns::Widget", "x").kind(set(), set(), _hooks) == "object"


def test_param_kind_unregistered_mutable_ref_with_no_canonical_stays_rejected():
    # No canonical (typedef-resolved) spelling available at all - a
    # return-type check builds a Param exactly this way (see Param.canonical's
    # own field docstring) - stays conservative rather than guessing whether
    # this is really a class or a fundamental hiding behind a typedef.
    assert generate.Param("Widget &", "x").kind(set(), set(), _hooks) is None


def test_param_kind_unregistered_mutable_ref_confirmed_class_is_accepted():
    # The canonical spelling positively confirms this isn't a fundamental -
    # same "stable Lua-side identity, safe to mutate in place" reasoning an
    # in-run registered class already gets (see the built-in registered_types
    # case above), now extended to an unregistered one too.
    assert generate.Param("Widget &", "x", canonical="Widget").kind(set(), set(), _hooks) == "object"


def test_param_kind_unregistered_mutable_ref_confirmed_fundamental_stays_rejected():
    # The canonical spelling reveals this is actually a fundamental hiding
    # behind an unfamiliar typedef (a different library's own "Standard_Real")
    # - a mutable reference to that would be silently wrong the same way a
    # bare fundamental's own mutable reference already is.
    assert generate.Param("Distance &", "x", canonical="double").kind(set(), set(), _hooks) is None


def test_param_kind_pointer_spelling_stays_rejected():
    # Not a plausible bare type name (BARE_TYPE_NAME doesn't match) - stays
    # rejected regardless of the new unregistered-class fallback.
    assert generate.Param("Widget *", "x").kind(set(), set(), _hooks) is None


def test_param_kind_typedef_hiding_a_pointer_stays_rejected():
    # "PWidget" here plays the role of OCCT's own "pointer to X" typedef
    # convention (e.g. BOPAlgo_PPaveFiller = BOPAlgo_PaveFiller*) - the bare
    # spelling alone looks like a plausible class name, but the canonical
    # (typedef-resolved) spelling reveals it's secretly a pointer, so this
    # stays rejected rather than pushing a raw pointer as if it were a plain
    # object (found the hard way).
    assert generate.Param("PWidget", "x", canonical="Widget *").kind(set(), set(), _hooks) is None
    # A genuine class typedef (canonical still a plain identifier) is unaffected.
    assert generate.Param("WidgetAlias", "x", canonical="Widget").kind(set(), set(), _hooks) == "object"


def test_param_kind_falls_through_to_hooks_classify_param():
    class _RecognizesEverything(generate.CodeGenerator):
        def classify_param(self, param, registered_types, registered_enums):
            return "custom"
    assert generate.Param("SomeExoticType *", "x").kind(set(), set(), _RecognizesEverything()) == "custom"
    assert generate.Param("SomeExoticType *", "x").kind(set(), set(), generate.CodeGenerator()) is None


# ---------------------------------------------------------------------------
# Callable.rejection_reason() / accepted_arities()
# ---------------------------------------------------------------------------

def _callable(params, return_spelling="void", is_static=False):
    return generate.Callable("Widget", "method", "Do", params, return_spelling, is_const=False, is_static=is_static)


def test_rejection_reason_none_when_all_supported():
    c = _callable([generate.Param("double", "x")])
    assert c.rejection_reason(set(), set(), _hooks) is None


def test_rejection_reason_reports_first_unsupported_param():
    c = _callable([generate.Param("Unsupported *", "p")])
    reason = c.rejection_reason(set(), set(), _hooks)
    assert reason is not None
    assert "Unsupported" in reason


def test_rejection_reason_reports_unsupported_return():
    c = _callable([generate.Param("double", "x")], return_spelling="Unsupported *")
    reason = c.rejection_reason(set(), set(), _hooks)
    assert reason is not None
    assert "Unsupported" in reason


def test_accepted_arities_all_or_nothing_when_no_defaults():
    c = _callable([generate.Param("double", "x"), generate.Param("double", "y")])
    assert c.accepted_arities(set(), set(), _hooks) == [2]


def test_accepted_arities_default_argument_expansion():
    # Do(double x, double y = 0.0) - both arities [1, 2] are reachable from Lua,
    # the shorter one relying on C++'s own default at the actual call site.
    c = _callable([
        generate.Param("double", "x"),
        generate.Param("double", "y", has_default=True),
    ])
    assert c.accepted_arities(set(), set(), _hooks) == [1, 2]


def test_accepted_arities_unsupported_leading_param_with_no_default_is_unreachable():
    # First param unsupported and not defaulted -> nothing is reachable, regardless
    # of a later defaulted-but-otherwise-fine parameter.
    c = _callable([
        generate.Param("Unsupported *", "p"),
        generate.Param("double", "y", has_default=True),
    ])
    assert c.accepted_arities(set(), set(), _hooks) == []


def test_accepted_arities_unsupported_return_type_rejects_every_arity():
    c = _callable([generate.Param("double", "x", has_default=True)], return_spelling="Unsupported *")
    assert c.accepted_arities(set(), set(), _hooks) == []


# ---------------------------------------------------------------------------
# ambiguity_safe_overload_order()
# ---------------------------------------------------------------------------

class _AmbiguityHooks(generate.CodeGenerator):
    OVERLOAD_AMBIGUITY_KINDS = {"dynamic"}


def test_ambiguity_safe_overload_order_prefers_fewer_ambiguity_kinds_within_same_arity():
    ambiguous = (_callable([generate.Param("Anything", "a")]), ["dynamic"])
    concrete = (_callable([generate.Param("Widget", "a")]), ["object"])
    ordered = generate.ambiguity_safe_overload_order([ambiguous, concrete], _AmbiguityHooks())
    assert ordered == [concrete, ambiguous]


def test_ambiguity_safe_overload_order_is_a_noop_with_default_empty_kinds():
    ambiguous = (_callable([generate.Param("Anything", "a")]), ["dynamic"])
    concrete = (_callable([generate.Param("Widget", "a")]), ["object"])
    ordered = generate.ambiguity_safe_overload_order([ambiguous, concrete], generate.CodeGenerator())
    assert ordered == [ambiguous, concrete]  # original order preserved - nothing in OVERLOAD_AMBIGUITY_KINDS


def test_ambiguity_safe_overload_order_is_stable_and_groups_by_arity_first():
    one_arg = (_callable([generate.Param("double", "a")]), ["passthrough"])
    two_args_a = (_callable([generate.Param("Anything", "a"), generate.Param("Anything", "b")]), ["dynamic", "dynamic"])
    two_args_b = (_callable([generate.Param("Anything", "a"), generate.Param("Widget", "b")]), ["dynamic", "object"])
    ordered = generate.ambiguity_safe_overload_order([two_args_a, one_arg, two_args_b], _AmbiguityHooks())
    # one_arg (arity 1) first, then the arity-2 group with fewer "dynamic" kinds first.
    assert ordered == [one_arg, two_args_b, two_args_a]


# ---------------------------------------------------------------------------
# emit_operator_lambda() - always generic, never needs a CodeGenerator override.
# ---------------------------------------------------------------------------

def test_emit_operator_lambda_unary():
    c = generate.Callable("Widget", "operator", "operator-", [], "Widget", is_const=True,
                           meta_function="unary_minus", operator_token="-")
    src = generate.emit_operator_lambda(c)
    assert "Widget const& self" in src
    assert "return -self;" in src


def test_emit_operator_lambda_binary_member_form():
    other = generate.Param("Widget const&", "other")
    c = generate.Callable("Widget", "operator", "operator+", [other], "Widget", is_const=True,
                           meta_function="addition", operator_token="+")
    src = generate.emit_operator_lambda(c)
    assert "return self + other;" in src


def test_emit_operator_lambda_reversed_operand_static_form():
    other = generate.Param("double", "other")
    c = generate.Callable("Widget", "operator", "operator+", [other], "Widget", is_const=False,
                           is_static=True, meta_function="addition", operator_token="+")
    src = generate.emit_operator_lambda(c)
    # Reversed-operand sibling: `other` is the first Lua-facing argument, self the second.
    assert "return other + self;" in src


# ---------------------------------------------------------------------------
# CodeGenerator - the built-in default behavior (no subclass involved).
# ---------------------------------------------------------------------------

def test_code_generator_classify_param_default_is_none():
    assert generate.CodeGenerator().classify_param(generate.Param("Anything", "x"), set(), set()) is None


def test_code_generator_classify_ctor_mode_default_is_none():
    assert generate.CodeGenerator().classify_ctor_mode("Foo", ["SomeBase"]) is None
    assert generate.CodeGenerator().classify_ctor_mode("Foo", []) is None

    class _WithNamedBaseCtorMode(generate.CodeGenerator):
        def classify_ctor_mode(self, class_name, base_chain):
            return "handle" if "RcBase" in base_chain else None
    assert _WithNamedBaseCtorMode().classify_ctor_mode("Foo", ["RcBase"]) == "handle"
    assert _WithNamedBaseCtorMode().classify_ctor_mode("Foo", ["OtherBase"]) is None


def test_code_generator_emit_param_default_is_none():
    assert generate.CodeGenerator().emit_param(generate.Param("double", "x"), "passthrough") is None


def test_code_generator_emit_constructor_default_raises_loudly():
    with pytest.raises(NotImplementedError):
        generate.CodeGenerator().emit_constructor("Widget", "custom_mode", [], "")


def test_code_generator_emit_bases_default_is_identity():
    assert generate.CodeGenerator().emit_bases("Foo", []) == []
    assert generate.CodeGenerator().emit_bases("Foo", ["A", "B"]) == ["A", "B"]


def test_code_generator_emit_return_type_default_is_none():
    assert generate.CodeGenerator().emit_return_type("Widget") is None


def test_emit_callable_plain_passthrough_needs_no_conversion():
    c = _callable([generate.Param("double", "x")], return_spelling="double")
    src = generate.CodeGenerator().emit_callable(c, ["passthrough"], "value")
    assert "double x" in src
    assert "self.Do(x)" in src


def test_emit_callable_constructor_value_mode():
    c = generate.Callable("Widget", "constructor", "Widget", [generate.Param("double", "x")], None, is_const=False)
    src = generate.CodeGenerator().emit_callable(c, ["passthrough"], "value")
    assert "return Widget(x);" in src


def test_emit_callable_constructor_unique_ptr_mode():
    c = generate.Callable("Widget", "constructor", "Widget", [], None, is_const=False)
    src = generate.CodeGenerator().emit_callable(c, [], "unique_ptr")
    assert "std::make_unique<Widget>()" in src


def test_emit_callable_constructor_custom_mode_without_override_raises():
    c = generate.Callable("Widget", "constructor", "Widget", [], None, is_const=False)
    with pytest.raises(NotImplementedError):
        generate.CodeGenerator().emit_callable(c, [], "handle")


def test_emit_callable_operator_ignores_ctor_mode_and_kinds():
    c = generate.Callable("Widget", "operator", "operator-", [], "Widget", is_const=True,
                           meta_function="unary_minus", operator_token="-")
    src = generate.CodeGenerator().emit_callable(c, [], "value")
    assert "return -self;" in src


# ---------------------------------------------------------------------------
# A deliberately toy, non-OCCT/ADOL-C-sounding CodeGenerator subclass - proves
# the hook-machinery plumbing itself (a custom kind, the "override emit_callable,
# call super() twice with private state" dual-emission composition pattern) works
# generically, without reproducing any real library's own literals.
# ---------------------------------------------------------------------------

class _ToyCodeGenerator(generate.CodeGenerator):
    OVERLOAD_AMBIGUITY_KINDS = {"widget"}
    _alternate = False  # private state this subclass manages - generate.py never sees it

    def classify_param(self, param, registered_types, registered_enums):
        if param.base == "FakeScalar":
            return None if param.is_mutable_ref else "widget"
        return None

    def emit_param(self, param, kind):
        if kind == "widget" and self._alternate:
            return f"sol::object {param.name}", f"toy_ns::to_widget({param.name})"
        return None  # primary variant: plain builtin fallback spelling

    def emit_callable(self, c, kinds, ctor_mode):
        if c.kind == "operator" or "widget" not in kinds:
            return super().emit_callable(c, kinds, ctor_mode)
        primary = super().emit_callable(c, kinds, ctor_mode)
        self._alternate = True
        try:
            alternate = super().emit_callable(c, kinds, ctor_mode)
        finally:
            self._alternate = False
        return f"\n#if defined(TOY_VARIANT_MACRO)\n{alternate}\n#else\n{primary}\n#endif\n"


def test_toy_code_generator_classify_param_recognizes_custom_kind():
    hooks = _ToyCodeGenerator()
    assert generate.Param("FakeScalar", "x").kind(set(), set(), hooks) == "widget"
    assert generate.Param("FakeScalar &", "x").kind(set(), set(), hooks) is None  # mutable-ref rejected


def test_toy_code_generator_emit_callable_dual_emission_composition():
    c = _callable([generate.Param("FakeScalar", "x")], return_spelling="double")
    src = _ToyCodeGenerator().emit_callable(c, ["widget"], "value")
    assert "#if defined(TOY_VARIANT_MACRO)" in src
    assert "toy_ns::to_widget(x)" in src  # alternate variant
    assert "FakeScalar x" in src  # primary variant, builtin fallback spelling


def test_toy_code_generator_emit_callable_skips_dual_emission_for_operators():
    other = generate.Param("FakeScalar", "other")
    c = generate.Callable("Widget", "operator", "operator+", [other], "Widget", is_const=True,
                           meta_function="addition", operator_token="+")
    src = _ToyCodeGenerator().emit_callable(c, ["widget"], "value")
    assert "TOY_VARIANT_MACRO" not in src


# ---------------------------------------------------------------------------
# load_code_generator()
# ---------------------------------------------------------------------------

def test_load_code_generator_absent_key_returns_base_class():
    hooks = generate.load_code_generator({}, Path("/nonexistent/config.yml"))
    assert type(hooks) is generate.CodeGenerator


def test_load_code_generator_missing_file_raises(tmp_path):
    config_path = tmp_path / "config.yml"
    with pytest.raises(RuntimeError, match="not found"):
        generate.load_code_generator({"code_generator": "missing.py"}, config_path)


def test_load_code_generator_loads_subclass_relative_to_config_path(tmp_path):
    (tmp_path / "hooks.py").write_text(
        "from grunk.codegen import generate\n"
        "class MyHooks(generate.CodeGenerator):\n"
        "    MARKER = 'loaded'\n"
    )
    hooks = generate.load_code_generator({"code_generator": "hooks.py"}, tmp_path / "config.yml")
    assert hooks.MARKER == "loaded"


def test_load_code_generator_zero_subclasses_raises(tmp_path):
    (tmp_path / "hooks.py").write_text("x = 1\n")
    with pytest.raises(RuntimeError, match="no CodeGenerator subclass"):
        generate.load_code_generator({"code_generator": "hooks.py"}, tmp_path / "config.yml")


def test_load_code_generator_multiple_subclasses_raises(tmp_path):
    (tmp_path / "hooks.py").write_text(
        "from grunk.codegen import generate\n"
        "class A(generate.CodeGenerator): pass\n"
        "class B(generate.CodeGenerator): pass\n"
    )
    with pytest.raises(RuntimeError, match="more than one"):
        generate.load_code_generator({"code_generator": "hooks.py"}, tmp_path / "config.yml")


# ---------------------------------------------------------------------------
# compute_translation_units()
# ---------------------------------------------------------------------------

def test_compute_translation_units_separate_mode_default_is_one_line_per_module():
    config = {}
    assert generate.compute_translation_units(config, ["gp", "geom"]) == ["gp", "geom"]


def test_compute_translation_units_groups_are_merged_into_one_line():
    config = {"translation_units": {"groups": [["gc", "geom", "geomapi"]]}}
    lines = generate.compute_translation_units(config, ["gc", "geom", "geomapi", "brep"])
    assert lines == ["gc geom geomapi", "brep"]


def test_compute_translation_units_unity_mode_returns_single_star():
    config = {"translation_units": {"mode": "unity"}}
    assert generate.compute_translation_units(config, ["gp", "geom"]) == ["*"]


def test_compute_translation_units_rejects_unknown_module_in_group():
    config = {"translation_units": {"groups": [["gp", "does_not_exist"]]}}
    with pytest.raises(ValueError, match="unknown module"):
        generate.compute_translation_units(config, ["gp"])


def test_compute_translation_units_rejects_module_listed_in_two_groups():
    config = {"translation_units": {"groups": [["gp"], ["gp", "geom"]]}}
    with pytest.raises(ValueError, match="more than one group"):
        generate.compute_translation_units(config, ["gp", "geom"])


def test_compute_translation_units_rejects_invalid_mode():
    config = {"translation_units": {"mode": "bogus"}}
    with pytest.raises(ValueError, match="mode"):
        generate.compute_translation_units(config, ["gp"])


# ---------------------------------------------------------------------------
# expand_headers()
# ---------------------------------------------------------------------------

def test_expand_headers_literal_filename(tmp_path):
    (tmp_path / "Widget.hpp").write_text("")
    headers, glob_matched = generate.expand_headers(["Widget.hpp"], tmp_path)
    assert headers == ["Widget.hpp"]
    assert glob_matched == set()


def test_expand_headers_missing_literal_filename_raises(tmp_path):
    with pytest.raises(RuntimeError, match="not found"):
        generate.expand_headers(["Missing.hpp"], tmp_path)


def test_expand_headers_glob_pattern_sorted_and_marked(tmp_path):
    (tmp_path / "widget_a.hxx").write_text("")
    (tmp_path / "widget_b.hxx").write_text("")
    headers, glob_matched = generate.expand_headers(["widget_*.hxx"], tmp_path)
    assert headers == ["widget_a.hxx", "widget_b.hxx"]  # sorted
    assert glob_matched == {"widget_a.hxx", "widget_b.hxx"}


def test_expand_headers_glob_no_match_raises(tmp_path):
    with pytest.raises(RuntimeError, match="matched no files"):
        generate.expand_headers(["widget_*.hxx"], tmp_path)


def test_expand_headers_exclude_headers_drops_glob_matches(tmp_path):
    (tmp_path / "widget_a.hxx").write_text("")
    (tmp_path / "widget_bad.hxx").write_text("")
    headers, _ = generate.expand_headers(["widget_*.hxx"], tmp_path, exclude_headers=["widget_bad.hxx"])
    assert headers == ["widget_a.hxx"]


def test_expand_headers_dedups_across_overlapping_patterns(tmp_path):
    (tmp_path / "widget_a.hxx").write_text("")
    headers, _ = generate.expand_headers(["widget_*.hxx", "widget_a.hxx"], tmp_path)
    assert headers == ["widget_a.hxx"]


# ---------------------------------------------------------------------------
# Integration: parse and generate registration code from a real, synthetic
# header via libclang, end-to-end through run() - the pure-logic tests above
# construct Param/Callable directly and so can never catch a regression in
# discover_entities/collect_class_callables/parse_headers themselves.
# ---------------------------------------------------------------------------

WIDGET_HEADER = """\
class Widget {
public:
    Widget(double value);
    double value() const;
    Widget operator+(Widget const& other) const;
    static double unit();
    void unsupported(void* p);
};
"""


def test_run_end_to_end_generates_expected_registration_code(tmp_path, capsys):
    """No `code_generator:` key at all - proves the zero-hooks path (a plugin
    like grunk-adolc's own adtl.h, whose headers never need anything beyond the
    generator's own built-in vocabulary) works standalone."""
    include_dir = tmp_path / "include"
    include_dir.mkdir()
    (include_dir / "widget.hpp").write_text(WIDGET_HEADER)

    config_path = tmp_path / "config.yml"
    config_path.write_text(yaml.safe_dump({
        "modules": [{"name": "widget", "headers": ["widget.hpp"]}],
    }))

    output_dir = tmp_path / "generated"
    args = argparse.Namespace(config=config_path, include_dir=include_dir, output_dir=output_dir)
    assert generate.run(args) == 0

    cpp_source = (output_dir / "widget.cpp").read_text()
    assert (output_dir / "widget.hpp").exists()
    assert "register_type<Widget" in cpp_source
    assert ".add_constructors(" in cpp_source
    assert '"value"' in cpp_source
    assert "sol::meta_function::addition" in cpp_source
    assert '"Widget_unit"' in cpp_source  # static method, composed name (Widget is a class, not a namespace)

    # No `generated_banner:` key - the generic, copyright-free default (see
    # DEFAULT_GENERATED_BANNER's own comment) is what actually gets written,
    # both here and in the .hpp.
    assert cpp_source.startswith(generate.DEFAULT_GENERATED_BANNER)
    assert (output_dir / "widget.hpp").read_text().startswith(generate.DEFAULT_GENERATED_BANNER)

    stderr = capsys.readouterr().err
    assert "rejected Widget::unsupported" in stderr

    tu_lines = (output_dir / "translation_units.txt").read_text().splitlines()
    assert tu_lines == ["widget"]


def test_run_end_to_end_uses_custom_generated_banner(tmp_path):
    """A `generated_banner:` key - each plugin's own SPDX/copyright header,
    since that's specific to each plugin's own authorship, not something this
    shared generator should assume (see DEFAULT_GENERATED_BANNER's own
    comment)."""
    include_dir = tmp_path / "include"
    include_dir.mkdir()
    (include_dir / "widget.hpp").write_text(WIDGET_HEADER)

    banner = (
        "// SPDX-FileCopyrightText: 2026 Some Plugin Author <author@example.com>\n"
        "//\n"
        "// SPDX-License-Identifier: Apache-2.0\n"
    )
    config_path = tmp_path / "config.yml"
    config_path.write_text(yaml.safe_dump({
        "generated_banner": banner,
        "modules": [{"name": "widget", "headers": ["widget.hpp"]}],
    }))

    output_dir = tmp_path / "generated"
    args = argparse.Namespace(config=config_path, include_dir=include_dir, output_dir=output_dir)
    assert generate.run(args) == 0

    cpp_source = (output_dir / "widget.cpp").read_text()
    hpp_source = (output_dir / "widget.hpp").read_text()
    assert cpp_source.startswith(banner)
    assert hpp_source.startswith(banner)
    assert generate.DEFAULT_GENERATED_BANNER not in cpp_source
    assert "Some Plugin Author" in cpp_source


HOOKED_WIDGET_HEADER = """\
using FakeScalar = double;

class Widget {
public:
    Widget(double value);
    double scale(FakeScalar factor) const;
};
"""


def test_run_end_to_end_loads_code_generator_from_config(tmp_path):
    """A `code_generator:` key pointing at a real file on disk - proves
    load_code_generator()'s own file-resolution/subclass-discovery machinery
    works through the full run() pipeline, not just in isolation."""
    include_dir = tmp_path / "include"
    include_dir.mkdir()
    (include_dir / "widget.hpp").write_text(HOOKED_WIDGET_HEADER)

    (tmp_path / "hooks.py").write_text(
        "from grunk.codegen import generate\n"
        "class Hooks(generate.CodeGenerator):\n"
        "    def classify_param(self, param, registered_types, registered_enums):\n"
        "        return 'widget' if param.base == 'FakeScalar' else None\n"
        "    def emit_param(self, param, kind):\n"
        "        if kind == 'widget':\n"
        "            return f'sol::object {param.name}', f'toy_ns::to_widget({param.name})'\n"
        "        return None\n"
    )

    config_path = tmp_path / "config.yml"
    config_path.write_text(yaml.safe_dump({
        "code_generator": "hooks.py",
        "modules": [{"name": "widget", "headers": ["widget.hpp"]}],
    }))

    output_dir = tmp_path / "generated"
    args = argparse.Namespace(config=config_path, include_dir=include_dir, output_dir=output_dir)
    assert generate.run(args) == 0

    cpp_source = (output_dir / "widget.cpp").read_text()
    assert "sol::object factor" in cpp_source
    assert "toy_ns::to_widget(factor)" in cpp_source
