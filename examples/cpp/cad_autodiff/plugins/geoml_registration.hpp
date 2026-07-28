// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
// SPDX-FileCopyrightText: 2026 Mladen Banocic <mladen.banovic@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

// register_geoml is shared verbatim by geoml/geoml_plugin.cpp (stage 2: CAD, no AD)
// and geoml_adolc/geoml_adolc_plugin.cpp (stage 3: CAD + AD, see README.md's
// "Stage 3" section). Per geoml's feature/autodiff branch, geoml's public API is
// identical either way - only what Standard_Real resolves to (double, vs. an
// ADOL-C adouble via https://github.com/dlr-sp/adOCCT) differs. Sharing one header
// between both plugins, instead of hand-copying it, is what actually proves that:
// one piece of C++ source, compiled against two different geoml/OCCT installs.
// GEOML_ADOLC_FORWARD/GEOML_ADOLC_REVERSE (defined by geoml's own CMakeLists.txt
// when GEOML_USE_ADOLC=ON) gate the handful of spots that do differ - the gp_Pnt
// constructor, and seed_x/curve_point_x/curve_point_dx_dX/curve_point_dy_dX below,
// which only exist in the AD build.
//
// Everything OCCT/geoml-related lives here and in occt_sol_traits.hpp - src/main.cpp
// never includes an OCCT header, not even to use export_brep's result below: it only
// ever sees grunk::state, recipes, and sol::object/bool.

#include <TColgp_Array1OfPnt.hxx>
#include <gp_Pnt.hxx>
#include <Geom_BezierCurve.hxx>
#include <BRepTools.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>

#include <geoml/curves/curves.h>
#include <geoml/surfaces/surfaces.h>
#include "geoml/data_structures/conversions.h"

#include <grunk/grunk.hpp>

#include "occt_sol_traits.hpp"

inline void register_geoml(grunk::state& grunk)
{
    grunk.register_type<gp_Pnt>("gp_Pnt")
    .add_constructors(
#if defined(GEOML_ADOLC_FORWARD) || defined(GEOML_ADOLC_REVERSE)
        // Standard_Real is Standard_Adouble here, not a fundamental type, so a single
        // (Standard_Real,Standard_Real,Standard_Real) overload (as in the non-AD
        // branch below) can't bind write_cad_recipe's `gp_Pnt.new(X, 0., 0.)` call
        // (shared verbatim with the non-AD build): X may be a seeded Standard_Adouble
        // userdata (see seed_x below) while the 0. literals stay plain Lua numbers,
        // so no single sol2 overload matches every argument at once - each argument
        // needs its own number-or-userdata check instead.
        [](sol::object x, sol::object y, sol::object z) {
            auto to_real = [](sol::object o) -> Standard_Real {
                return o.is<Standard_Real>() ? o.as<Standard_Real>() : Standard_Real(o.as<double>());
            };
            return gp_Pnt(to_real(x), to_real(y), to_real(z));
        }
#else
        [](Standard_Real x, Standard_Real y, Standard_Real z) { return gp_Pnt(x,y,z); }
#endif
    )
    .with_std_vector();

    //grunk.register_type<Geom_Curve, sol::automagic_flags::none>("Geom_Curve")
    //.with_std_vector();

    //grunk.register_type<Geom_BezierCurve, sol::automagic_flags::none>("Geom_BezierCurve")
    //.add_bases<Geom_Curve>();

    //grunk.register_type<Geom_Surface>("Geom_Surface");

    //grunk.register_type<Geom_BSplineSurface>("Geom_BSplineSurface")
    //.add_bases<Geom_Surface>();

    grunk.register_function(
        "bezier_curve",
        [](std::vector<gp_Pnt> const& poles) -> Handle(Geom_BezierCurve) {
            TColgp_Array1OfPnt occ_poles = geoml::StdVector_to_TCol(poles);
            return new Geom_BezierCurve(occ_poles);
        }
    );

    grunk.register_function("interpolate_curve_network", geoml::interpolate_curve_network);

    // Lets a recipe verify/export a curve it built without src/main.cpp ever naming
    // an OCCT type - see this header's file comment.
    grunk.register_function(
        "export_brep",
        [](Handle(Geom_BezierCurve) const& curve, std::string const& filename) -> bool {
            BRepTools::Write(BRepBuilderAPI_MakeEdge(curve), filename.c_str());
            return true;
        }
    );

#if defined(GEOML_ADOLC_FORWARD) || defined(GEOML_ADOLC_REVERSE)
    // Stage 3 only: seeding X's derivative direction and pulling a derivative back
    // out of the resulting geometry. Neither call is part of write_cad_recipe's
    // shared recipe text - src/main.cpp's read_recipe_ad appends them itself (seed_x
    // via DynamicFeature::set_value on X's existing node, curve_point_x/
    // curve_point_dx_dX via an appended recipe.eval), so the recipe stays identical
    // between stages 2 and 3. See README.md's "Stage 3" section.

    // Adapts stage 1's `me.seed(x_val)` Lua helper (src/main.cpp,
    // write_autodiff_only_recipe) to a Standard_Adouble: wraps x as an adouble and
    // seeds derivative direction 0, the same direction curve_point_dx_dX below reads
    // back. x arrives as a plain double (X's Lua-visible value is still a plain
    // number at read time - see read_recipe_ad), not a Standard_Real, since nothing
    // has produced a Standard_Adouble for it yet; that's this function's job.
    grunk.register_function(
        "seed_x",
        [](double x) -> Standard_Real {
            Standard_Adouble seeded(x);
            seeded.setADValue(0, 1.);
            return seeded;
        }
    );

    // Samples the curve's X coordinate at parameter u and returns its primal value -
    // same information export_brep's caller could get from the .brep file, but
    // in-process and without a round trip through disk. u is a plain double, not
    // Standard_Real: it's just the curve parameter (not something seeded - no
    // derivative is ever needed with respect to it), and recipe.eval always passes
    // it as a plain Lua number literal (see read_recipe_ad), which sol2 can't bind
    // directly to a Standard_Adouble parameter (same gap gp_Pnt's constructor has).
    grunk.register_function(
        "curve_point_x",
        [](Handle(Geom_BezierCurve) const& curve, double u) -> double {
            return curve->Value(Standard_Adouble(u)).X().getValue();
        }
    );

    // The payoff: d(curve point's X coordinate)/dX at parameter u, read directly off
    // the AD tape - direction 0 matches seed_x above. Requires X to have actually
    // been seeded (see read_recipe_ad); otherwise this is trivially 0.
    grunk.register_function(
        "curve_point_dx_dX",
        [](Handle(Geom_BezierCurve) const& curve, double u) -> double {
            return curve->Value(Standard_Adouble(u)).X().getADValue(0);
        }
    );

    // Sanity check alongside curve_point_dx_dX: X only ever feeds P_1's X
    // coordinate (write_cad_recipe's `P_1 = gp_Pnt.new(X, 0., 0.)`), so the curve
    // point's Y coordinate should carry no dependency on X at all - this should
    // read exactly 0 wherever curve_point_dx_dX is nonzero, confirming the AD tape
    // isn't leaking a derivative into an unrelated component.
    grunk.register_function(
        "curve_point_dy_dX",
        [](Handle(Geom_BezierCurve) const& curve, double u) -> double {
            return curve->Value(Standard_Adouble(u)).Y().getADValue(0);
        }
    );
#endif
}
