# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

"""Tests for grunk.codegen.generate - the shared libclang-based code generator
used by grunk-occt/grunk-adolc (see its own module docstring). Split into two
parts: pure-logic unit tests that construct Param/Callable dataclasses directly
(no libclang parsing involved - covers the string-emission/acceptance rules
generate.py's own header-parsing layer feeds into) and a small integration test
that parses a real, synthetic header via libclang end-to-end through run(), to
catch a regression in the parsing layer itself that the pure-logic tests, by
construction, cannot see."""

import argparse

import yaml
import pytest

from grunk.codegen import generate


# ---------------------------------------------------------------------------
# strip_type / array element helpers
# ---------------------------------------------------------------------------

def test_strip_type_removes_const_and_reference():
    assert generate.strip_type("const gp_Vec &") == "gp_Vec"
    assert generate.strip_type("gp_Vec") == "gp_Vec"
    assert generate.strip_type("Standard_Real") == "Standard_Real"
    assert generate.strip_type("const   double  &") == "double"


def test_array_element_type_extracts_template_argument():
    assert generate.array_element_type("const NCollection_Array1<gp_Pnt> &", generate.ARRAY1_PATTERN) == "gp_Pnt"
    assert generate.array_element_type("NCollection_Array2<double>", generate.ARRAY2_PATTERN) == "double"


def test_array_element_supported():
    registered = {"gp_Pnt"}
    assert generate.array_element_supported("double", registered)  # REAL_ELEMENT_SPELLINGS
    assert generate.array_element_supported("int", registered)  # PASSTHROUGH_ELEMENT_SPELLINGS
    assert generate.array_element_supported("gp_Pnt", registered)  # registered_types
    assert not generate.array_element_supported("gp_Ax2", registered)


# ---------------------------------------------------------------------------
# Param.kind()
# ---------------------------------------------------------------------------

def test_param_kind_real_and_passthrough():
    assert generate.Param("Standard_Real", "x").kind(set(), set()) == "real"
    assert generate.Param("Standard_Real &", "x").kind(set(), set()) is None  # mutable-ref real rejected
    assert generate.Param("double", "x").kind(set(), set()) == "passthrough"
    assert generate.Param("bool &", "x").kind(set(), set()) is None


def test_param_kind_registered_enum_and_type():
    assert generate.Param("GeomAbs_Shape", "x").kind(set(), {"GeomAbs_Shape"}) == "passthrough"
    assert generate.Param("gp_Pnt", "x").kind({"gp_Pnt"}, set()) == "object"
    assert generate.Param("gp_Pnt &", "x").kind({"gp_Pnt"}, set()) == "object"  # mutable "out" object param IS supported
    assert generate.Param("gp_Pnt", "x").kind(set(), set()) is None  # not registered -> unsupported


def test_param_kind_handle():
    assert generate.Param("opencascade::handle<Geom_Curve>", "x").kind({"Geom_Curve"}, set()) == "handle"
    assert generate.Param("opencascade::handle<Geom_Curve>", "x").kind(set(), set()) is None  # Geom_Curve not registered


def test_param_kind_array1_array2():
    p1 = generate.Param("const TColgp_Array1OfPnt &", "pts", "const NCollection_Array1<gp_Pnt> &")
    assert p1.kind({"gp_Pnt"}, set()) == "array1"
    assert p1.kind(set(), set()) is None  # gp_Pnt not registered -> element unsupported

    p2 = generate.Param("const TColStd_Array2OfReal &", "m", "const NCollection_Array2<double> &")
    assert p2.kind(set(), set()) == "array2"  # double is always supported (REAL_ELEMENT_SPELLINGS)

    p_mut = generate.Param("TColgp_Array1OfPnt &", "pts", "NCollection_Array1<gp_Pnt> &")
    assert p_mut.kind({"gp_Pnt"}, set()) is None  # mutable array ref rejected, no Lua-side identity to mutate


def test_param_kind_return_type_uses_empty_name():
    # rejection_reason/accepted_arities build a synthetic Param(spelling, "") to check
    # a *return* type - array kinds are deliberately parameter-only (see Param.kind's
    # own comment), so an array-typed return must stay unsupported even though the
    # element itself would otherwise qualify.
    ret = generate.Param("NCollection_Array1<gp_Pnt>", "", "NCollection_Array1<gp_Pnt>")
    assert ret.kind({"gp_Pnt"}, set()) is None


# ---------------------------------------------------------------------------
# Callable.rejection_reason() / accepted_arities()
# ---------------------------------------------------------------------------

def _callable(params, return_spelling="void", is_static=False):
    return generate.Callable("Widget", "method", "Do", params, return_spelling, is_const=False, is_static=is_static)


def test_rejection_reason_none_when_all_supported():
    c = _callable([generate.Param("double", "x")])
    assert c.rejection_reason(set(), set()) is None


def test_rejection_reason_reports_first_unsupported_param():
    c = _callable([generate.Param("gp_Pnt", "p")])
    reason = c.rejection_reason(set(), set())
    assert reason is not None
    assert "gp_Pnt" in reason


def test_rejection_reason_reports_unsupported_return():
    c = _callable([generate.Param("double", "x")], return_spelling="gp_Pnt")
    reason = c.rejection_reason(set(), set())
    assert reason is not None
    assert "gp_Pnt" in reason


def test_accepted_arities_all_or_nothing_when_no_defaults():
    c = _callable([generate.Param("double", "x"), generate.Param("double", "y")])
    assert c.accepted_arities(set(), set()) == [2]


def test_accepted_arities_default_argument_expansion():
    # Do(double x, double y = 0.0) - both arities [1, 2] are reachable from Lua,
    # the shorter one relying on C++'s own default at the actual call site.
    c = _callable([
        generate.Param("double", "x"),
        generate.Param("double", "y", has_default=True),
    ])
    assert c.accepted_arities(set(), set()) == [1, 2]


def test_accepted_arities_unsupported_leading_param_with_no_default_is_unreachable():
    # First param unsupported and not defaulted -> nothing is reachable, regardless
    # of a later defaulted-but-otherwise-fine parameter.
    c = _callable([
        generate.Param("gp_Pnt", "p"),
        generate.Param("double", "y", has_default=True),
    ])
    assert c.accepted_arities(set(), set()) == []


def test_accepted_arities_unsupported_return_type_rejects_every_arity():
    c = _callable([generate.Param("double", "x", has_default=True)], return_spelling="gp_Pnt")
    assert c.accepted_arities(set(), set()) == []


# ---------------------------------------------------------------------------
# ambiguity_safe_overload_order()
# ---------------------------------------------------------------------------

def test_ambiguity_safe_overload_order_prefers_fewer_real_kinds_within_same_arity():
    all_real = (_callable([generate.Param("Standard_Real", "a")]), ["real"])
    concrete = (_callable([generate.Param("gp_Ax2", "a")]), ["object"])
    ordered = generate.ambiguity_safe_overload_order([all_real, concrete])
    assert ordered == [concrete, all_real]


def test_ambiguity_safe_overload_order_is_stable_and_groups_by_arity_first():
    one_arg = (_callable([generate.Param("double", "a")]), ["passthrough"])
    two_args_a = (_callable([generate.Param("Standard_Real", "a"), generate.Param("Standard_Real", "b")]), ["real", "real"])
    two_args_b = (_callable([generate.Param("Standard_Real", "a"), generate.Param("gp_Ax2", "b")]), ["real", "object"])
    ordered = generate.ambiguity_safe_overload_order([two_args_a, one_arg, two_args_b])
    # one_arg (arity 1) first, then the arity-2 group with fewer "real" kinds first.
    assert ordered == [one_arg, two_args_b, two_args_a]


# ---------------------------------------------------------------------------
# emit_lambda() / emit_callable_expr() - helper_namespace threading
# ---------------------------------------------------------------------------

def test_emit_lambda_plain_passthrough_has_no_helper_namespace_call():
    c = _callable([generate.Param("double", "x")], return_spelling="double")
    src = generate.emit_lambda(c, ["passthrough"], ad_branch=False, ctor_mode="value", helper_namespace="my_plugin")
    assert "my_plugin::" not in src
    assert "double x" in src


def test_emit_lambda_real_ad_branch_uses_helper_namespace_to_real():
    c = _callable([generate.Param("Standard_Real", "x")], return_spelling="double")
    src = generate.emit_lambda(c, ["real"], ad_branch=True, ctor_mode="value", helper_namespace="my_plugin")
    assert "my_plugin::to_real(x)" in src
    assert "sol::object x" in src


def test_emit_lambda_array1_real_element_uses_to_real_array1():
    c = _callable([generate.Param("const TColgp_Array1OfPnt &", "pts", "const NCollection_Array1<double> &")], return_spelling="void")
    src = generate.emit_lambda(c, ["array1"], ad_branch=False, ctor_mode="value", helper_namespace="occt_plugin")
    assert "occt_plugin::to_real_array1(pts)" in src


def test_emit_lambda_array1_non_real_element_uses_plain_to_array1():
    c = _callable([generate.Param("const TColgp_Array1OfPnt &", "pts", "const NCollection_Array1<gp_Pnt> &")], return_spelling="void")
    src = generate.emit_lambda(c, ["array1"], ad_branch=False, ctor_mode="value", helper_namespace="occt_plugin")
    assert "occt_plugin::to_array1(pts)" in src
    assert "to_real_array1" not in src


def test_emit_lambda_array2_uses_flat_rows_cols_and_helper_namespace():
    c = _callable([generate.Param("const TColStd_Array2OfReal &", "m", "const NCollection_Array2<double> &")], return_spelling="void")
    src = generate.emit_lambda(c, ["array2"], ad_branch=False, ctor_mode="value", helper_namespace="adolc_plugin")
    assert "adolc_plugin::to_real_array2(m_flat, m_rows, m_cols)" in src
    assert "m_flat" in src and "m_rows" in src and "m_cols" in src


def test_emit_lambda_constructor_handle_mode_heap_allocates():
    c = generate.Callable("Geom_Curve", "constructor", "Geom_Curve", [], None, is_const=False)
    src = generate.emit_lambda(c, [], ad_branch=False, ctor_mode="handle", helper_namespace="occt_plugin")
    assert "opencascade::handle<Geom_Curve>" in src
    assert "new Geom_Curve(" in src


def test_emit_callable_expr_wraps_ad_macro_only_when_real_present():
    plain_only = _callable([generate.Param("double", "x")], return_spelling="double")
    src = generate.emit_callable_expr(plain_only, ["passthrough"], ctor_mode="value", helper_namespace="occt_plugin")
    assert generate.AD_MACRO not in src

    with_real = _callable([generate.Param("Standard_Real", "x")], return_spelling="double")
    src = generate.emit_callable_expr(with_real, ["real"], ctor_mode="value", helper_namespace="occt_plugin")
    assert f"#if defined({generate.AD_MACRO})" in src
    assert "occt_plugin::to_real(x)" in src  # the AD branch
    assert "Standard_Real x" in src  # the plain (non-AD) branch keeps the bare Standard_Real spelling


# ---------------------------------------------------------------------------
# emit_operator_lambda()
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
    (tmp_path / "gp_Vec.hxx").write_text("")
    (tmp_path / "gp_Pnt.hxx").write_text("")
    headers, glob_matched = generate.expand_headers(["gp_*.hxx"], tmp_path)
    assert headers == ["gp_Pnt.hxx", "gp_Vec.hxx"]  # sorted
    assert glob_matched == {"gp_Pnt.hxx", "gp_Vec.hxx"}


def test_expand_headers_glob_no_match_raises(tmp_path):
    with pytest.raises(RuntimeError, match="matched no files"):
        generate.expand_headers(["gp_*.hxx"], tmp_path)


def test_expand_headers_exclude_headers_drops_glob_matches(tmp_path):
    (tmp_path / "gp_Vec.hxx").write_text("")
    (tmp_path / "gp_Bad.hxx").write_text("")
    headers, _ = generate.expand_headers(["gp_*.hxx"], tmp_path, exclude_headers=["gp_Bad.hxx"])
    assert headers == ["gp_Vec.hxx"]


def test_expand_headers_dedups_across_overlapping_patterns(tmp_path):
    (tmp_path / "gp_Vec.hxx").write_text("")
    headers, _ = generate.expand_headers(["gp_*.hxx", "gp_Vec.hxx"], tmp_path)
    assert headers == ["gp_Vec.hxx"]


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
    include_dir = tmp_path / "include"
    include_dir.mkdir()
    (include_dir / "widget.hpp").write_text(WIDGET_HEADER)

    config_path = tmp_path / "config.yml"
    config_path.write_text(yaml.safe_dump({
        "helper_namespace": "test_plugin",
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

    stderr = capsys.readouterr().err
    assert "rejected Widget::unsupported" in stderr

    tu_lines = (output_dir / "translation_units.txt").read_text().splitlines()
    assert tu_lines == ["widget"]
