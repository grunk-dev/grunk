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
//
// Both callers register into the same "geoml" namespace (see grunk::state::begin_plugin
// and each plugin's own grunk_plugin_info) - the whole point of stage 3 is that the
// *same* recipe text (src/main.cpp's write_cad_recipe) resolves against whichever one
// is actually loaded, so both must report the same PluginInfo::name.
// GEOML_ADOLC_FORWARD/GEOML_ADOLC_REVERSE (defined by geoml's own CMakeLists.txt
// when GEOML_USE_ADOLC=ON) gate the handful of spots that do differ - the gp_Pnt
// constructor's and Geom_BezierCurve::Value's argument conversion, and the
// Standard_Real type registration, which only exists in the AD build (in the
// non-AD build, Standard_Real is plain double - already a native Lua number, no
// registration needed).
//
// This plugin registers only genuine OCCT/geoml operations - gp_Pnt/Geom_BezierCurve
// construction, coordinate access and evaluation, curve export, and (AD-only)
// Standard_Real's own AD introspection (getValue/getADValue/setADValue, the same
// operations ADOL-C's adouble exposes). Anything specific to *this example* (e.g.
// seeding X's derivative direction, or reading a particular coordinate's
// derivative) is composed from these in Lua instead - see write_cad_recipe's
// recipe steps and read_recipe_ad's "ad" module script (src/main.cpp) - so this
// header stays reusable by any recipe, not tinkered for one.
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

inline void register_geoml(grunk::state& grunk, sol::table const& ns, grunk::PluginInfo const& info)
{
    grunk.register_type<gp_Pnt>("gp_Pnt", ns, info.name)
    .add_constructors(
#if defined(GEOML_ADOLC_FORWARD) || defined(GEOML_ADOLC_REVERSE)
        // Standard_Real is Standard_Adouble here, not a fundamental type, so a single
        // (Standard_Real,Standard_Real,Standard_Real) overload (as in the non-AD
        // branch below) can't bind a call mixing a seeded Standard_Real argument
        // (see the "ad" Lua module, src/main.cpp) with plain Lua number literals -
        // no single sol2 overload matches every argument at once, so each argument
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
    .add_member_function("X", &gp_Pnt::X)
    .add_member_function("Y", &gp_Pnt::Y)
    .add_member_function("Z", &gp_Pnt::Z)
    .with_std_vector();

    // automagic_flags::none: Geom_BezierCurve has no operator==/operator<, which
    // sol2's default usertype registration wants (for a Handle-wrapped/unique_usertype
    // type in particular) - this opts out of that (and of the other automagic
    // enrollments: default constructor, __tostring, __pairs, __call, __len -
    // Geom_BezierCurve needs none of them; construction happens exclusively via
    // bezier_curve below).
    grunk.register_type<Geom_BezierCurve, sol::automagic_flags::none>("Geom_BezierCurve", ns, info.name)
    .add_member_function("Value",
#if defined(GEOML_ADOLC_FORWARD) || defined(GEOML_ADOLC_REVERSE)
        // Same gap as gp_Pnt's constructor: u is always a plain Lua number, but
        // Standard_Real is Standard_Adouble here, not a fundamental type, so sol2
        // can't bind it directly to Geom_Curve::Value's Standard_Real parameter.
        [](Geom_BezierCurve const& self, double u) -> gp_Pnt { return self.Value(Standard_Adouble(u)); }
#else
        &Geom_Curve::Value
#endif
    );

    //grunk.register_type<Geom_Surface>("Geom_Surface");

    //grunk.register_type<Geom_BSplineSurface>("Geom_BSplineSurface")
    //.add_bases<Geom_Surface>();

    grunk.register_function(
        "bezier_curve",
        [](std::vector<gp_Pnt> const& poles) -> Handle(Geom_BezierCurve) {
            TColgp_Array1OfPnt occ_poles = geoml::StdVector_to_TCol(poles);
            return new Geom_BezierCurve(occ_poles);
        },
        {}, ns, info.name
    );

    grunk.register_function("interpolate_curve_network", geoml::interpolate_curve_network, {}, ns, info.name);

    // Lets a recipe verify/export a curve it built without src/main.cpp ever naming
    // an OCCT type - see this header's file comment.
    grunk.register_function(
        "export_brep",
        [](Handle(Geom_BezierCurve) const& curve, std::string const& filename) -> bool {
            BRepTools::Write(BRepBuilderAPI_MakeEdge(curve), filename.c_str());
            return true;
        },
        {}, ns, info.name
    );

#if defined(GEOML_ADOLC_FORWARD) || defined(GEOML_ADOLC_REVERSE)
    // AD build only: exposes Standard_Real (== Standard_Adouble here) itself as a
    // Lua-constructible/inspectable type, the same AD introspection ADOL-C's own
    // adtl::adouble gives Lua in stage 1 (write_autodiff_only_recipe's
    // load_adolc_plugin: getValue/getADValue/setADValue). This is genuine,
    // reusable OCCT/ADOL-C functionality - not specific to X or to this recipe -
    // gp_Pnt::X()/Y()/Z() above already return Standard_Real values that need
    // exactly these operations to inspect from Lua. getADValue/setADValue take an
    // AD direction index; ADOL-C's own signature is `unsigned int`, adapted to a
    // plain Lua number here.
    grunk.register_type<Standard_Real>("Standard_Real", ns, info.name)
    .add_constructors(
        [](double v) -> Standard_Real { return Standard_Adouble(v); }
    )
    .add_member_function("getValue", [](Standard_Adouble const& self) -> double {
        return self.getValue();
    })
    .add_member_function("getADValue", [](Standard_Adouble const& self, int direction) -> double {
        return self.getADValue(static_cast<unsigned int>(direction));
    })
    .add_member_function("setADValue", [](Standard_Adouble& self, int direction, double value) {
        self.setADValue(static_cast<unsigned int>(direction), value);
    });
#endif
}
