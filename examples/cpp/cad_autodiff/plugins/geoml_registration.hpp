// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
// SPDX-FileCopyrightText: 2026 Mladen Banocic <mladen.banovic@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

// register_geoml is shared verbatim by geoml/geoml_plugin.cpp (stage 2: CAD, no AD)
// and geoml_adolc/geoml_adolc_plugin.cpp (stage 3: CAD + AD - TODO(stage3), see
// README.md's "Stage 3" section). Per geoml's feature/autodiff branch, geoml's public
// API is identical either way - only what Standard_Real resolves to (double, vs. an
// ADOL-C adouble via https://github.com/dlr-sp/adOCCT) differs. Sharing one header
// between both plugins, instead of hand-copying it, is what actually proves that:
// one piece of C++ source, compiled against two different geoml/OCCT installs.
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
        [](Standard_Real x, Standard_Real y, Standard_Real z) { return gp_Pnt(x,y,z); }
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
}
